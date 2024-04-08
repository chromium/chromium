// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/platform_model_loader_chromeos.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/on_device_model_service.h"

// The structure of the base model package:
//
// 6c2d5dc9-32c3-4642-9ea3-3dc9cdf3854d:
//   model.json
//   model.pb
//   weights.bin
//   spm.model
//
// The model.json content:
// {
//     "name": "My base model",
//     "version": "1.0.0.0",
//     "max_tokens": 1024,
//     "adaptation_ranks": [32],
//     "model_path": "model.pb",
//     "weight_path": "weights.bin",
//     "sp_model_path": "spm.model"
// }
//
// The structure of the adaptation layer model package:
//
// 75079ea6-c55a-44df-acce-7ac4cc861ee1:
//   model.json
//   model.pb
//   weights.bin
//
// The model.json content:
// {
//     "name": "My adaptation model",
//     "version": "2.0.0.1",
//     "model_path": "model.pb",
//     "weight_path": "weights.bin",
//     "base_model": {
//         "uuid": "6c2d5dc9-32c3-4642-9ea3-3dc9cdf3854d",
//         "version": "1.0.0.0"
//     }
// }

namespace {
constexpr char kMlDlcPrefix[] = "ml-dlc-";
constexpr char kModelDescriptor[] = "model.json";
constexpr char kBaseModelKey[] = "base_model";
constexpr char kUuidKey[] = "uuid";
constexpr char kMaxTokensKey[] = "max_tokens";
constexpr char kAdaptationRanksKey[] = "adaptation_ranks";
constexpr char kModelPathKey[] = "model_path";
constexpr char kWeightPathKey[] = "weight_path";
constexpr char kSpModelPathKey[] = "sp_model_path";
constexpr char kVersionKey[] = "version";
constexpr int kDefaultMaxTokens = 1024;
constexpr char kLoadStatusHistogramName[] =
    "OnDeviceModel.LoadPlatformModelStatus";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LoadStatus {
  kFirstLoadSuccess = 0,
  kLoadExistingSuccess = 1,
  kInvalidUuid = 2,
  kNoDlcClient = 3,
  kInstallDlcFail = 4,
  kReadModelDescriptorFail = 5,
  kParseModelDescriptorFail = 6,
  kInvalidModelDescriptor = 7,
  kInvalidBaseModelDescriptor = 8,
  kLoadModelFail = 9,
  kLoadBaseModelFail = 10,
  kInvalidModelVersion = 11,
  kMaxValue = kInvalidModelVersion,
};
}  // namespace

namespace on_device_model {

ChromeosPlatformModelLoader::PlatformModel::PlatformModel() = default;
ChromeosPlatformModelLoader::PlatformModel::~PlatformModel() = default;

// static
bool ChromeosPlatformModelLoader::PlatformModelRefTraits::IsNull(
    const PointerType& ptr) {
  return !ptr && !ptr.get() && !ptr.get()->cur_model().get();
}

// static
mojom::OnDeviceModel*
ChromeosPlatformModelLoader::PlatformModelRefTraits::GetRawPointer(
    PointerType* ptr) {
  return ptr->get()->cur_model().get();
}

ChromeosPlatformModelLoader::PendingLoad::PendingLoad(
    mojo::PendingReceiver<mojom::OnDeviceModel> p,
    LoadModelCallback c)
    : pending(std::move(p)), callback(std::move(c)) {}
ChromeosPlatformModelLoader::PendingLoad::PendingLoad(PendingLoad&&) = default;
ChromeosPlatformModelLoader::PendingLoad::~PendingLoad() = default;

ChromeosPlatformModelLoader::PlatformModelRecord::PlatformModelRecord() =
    default;
ChromeosPlatformModelLoader::PlatformModelRecord::~PlatformModelRecord() =
    default;

ChromeosPlatformModelLoader::ChromeosPlatformModelLoader(
    mojom::OnDeviceModelService& service)
    : service_(service) {}
ChromeosPlatformModelLoader::~ChromeosPlatformModelLoader() = default;

bool ChromeosPlatformModelLoader::ReplyModelAlreadyLoaded(
    const base::Uuid& uuid) {
  auto it = platform_models_.find(uuid);
  if (it == platform_models_.end()) {
    return false;
  }

  if (!it->second.platform_model) {
    return false;
  }

  PlatformModelRecord& record = it->second;

  std::vector<PendingLoad> pending_loads = std::move(record.pending_loads);
  record.pending_loads.clear();

  for (auto& pending_load : pending_loads) {
    receivers_.Add(base::WrapRefCounted(record.platform_model.get()),
                   std::move(pending_load.pending));
    std::move(pending_load.callback).Run(mojom::LoadModelResult::kSuccess);
  }

  return true;
}

void ChromeosPlatformModelLoader::ReplyError(const base::Uuid& uuid,
                                             mojom::LoadModelResult result) {
  CHECK(result != mojom::LoadModelResult::kSuccess);

  auto it = platform_models_.find(uuid);
  if (it == platform_models_.end()) {
    return;
  }

  PlatformModelRecord& record = it->second;

  std::vector<PendingLoad> pending_loads = std::move(record.pending_loads);
  record.pending_loads.clear();

  for (auto& pending_load : pending_loads) {
    std::move(pending_load.callback).Run(result);
  }
}

void ChromeosPlatformModelLoader::LoadModelWithUuid(
    const base::Uuid& uuid,
    mojo::PendingReceiver<mojom::OnDeviceModel> pending,
    LoadModelCallback callback) {
  if (!uuid.is_valid()) {
    LOG(ERROR) << "Invalid model UUID";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kInvalidUuid);
    std::move(callback).Run(mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  platform_models_[uuid].pending_loads.push_back(
      PendingLoad(std::move(pending), std::move(callback)));

  if (ReplyModelAlreadyLoaded(uuid)) {
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kLoadExistingSuccess);
    return;
  }

  if (platform_models_[uuid].pending_loads.size() > 1) {
    // Someone else is already loading the model.
    return;
  }

  std::string uuid_str = uuid.AsLowercaseString();
  dlcservice::InstallRequest request;
  request.set_id(kMlDlcPrefix + uuid_str);
  ash::DlcserviceClient* client = ash::DlcserviceClient::Get();

  if (!client) {
    LOG(ERROR) << "Failed to get DlcserviceClient";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kNoDlcClient);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  // TODO(b/331050878): Add another interface to report the progress.
  client->Install(
      request,
      base::BindOnce(&ChromeosPlatformModelLoader::OnInstallDlcComplete,
                     AsWeakPtr(), uuid),
      /*ProgressCallback=*/base::DoNothing());
  return;
}

void ChromeosPlatformModelLoader::OnInstallDlcComplete(
    const base::Uuid& uuid,
    const ash::DlcserviceClient::InstallResult& result) {
  if (result.error != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to install ML DLC with error " << result.error;
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kInstallDlcFail);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  base::FilePath dlc_root(result.root_path);
  base::FilePath model_desc = dlc_root.Append(kModelDescriptor);
  std::string model_json;

  if (!base::ReadFileToString(model_desc, &model_json)) {
    LOG(ERROR) << "Failed to read model descriptor file";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kReadModelDescriptorFail);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  std::optional<base::Value::Dict> model_dict =
      base::JSONReader::ReadDict(model_json);

  if (!model_dict) {
    LOG(ERROR) << "Failed to parse model descriptor file";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kParseModelDescriptorFail);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  const std::string* model_path = model_dict->FindString(kModelPathKey);
  const std::string* weight_path = model_dict->FindString(kWeightPathKey);
  const std::string* version = model_dict->FindString(kVersionKey);

  if (!model_path || !weight_path || !version) {
    LOG(ERROR) << "Failed to read model data from model descriptor file";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kInvalidModelDescriptor);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  const base::Value::Dict* base_model = model_dict->FindDict(kBaseModelKey);

  if (base_model) {
    // This is an adaptation layer model. We need to load the base model first.
    const std::string* base_uuid = base_model->FindString(kUuidKey);
    const std::string* base_version = base_model->FindString(kVersionKey);
    if (!base_uuid || !base_version) {
      LOG(ERROR) << "Failed to read base model data from model descriptor file";
      base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                    LoadStatus::kInvalidBaseModelDescriptor);
      ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
      return;
    }

    base::Uuid base_model_uuid = base::Uuid::ParseLowercase(*base_uuid);

    auto platform_model = base::MakeRefCounted<PlatformModel>();
    LoadModelWithUuid(
        base_model_uuid,
        platform_model->base_model().BindNewPipeAndPassReceiver(),
        base::BindOnce(
            &ChromeosPlatformModelLoader::LoadAdaptationPlatformModel,
            AsWeakPtr(), base_model_uuid, *base_version, uuid, dlc_root,
            *version, *model_path, *weight_path, std::move(platform_model)));

    return;
  }

  const std::string* sp_model = model_dict->FindString(kSpModelPathKey);

  if (!sp_model) {
    LOG(ERROR) << "Failed to read sp model path from model descriptor file";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kInvalidModelDescriptor);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  std::optional<int> max_tokens = model_dict->FindInt(kMaxTokensKey);

  const base::Value::List* ada_list = model_dict->FindList(kAdaptationRanksKey);
  std::vector<uint32_t> adaptation_ranks;
  if (ada_list) {
    for (auto& ada : *ada_list) {
      std::optional<int> rank = ada.GetIfInt();
      if (rank) {
        adaptation_ranks.push_back(*rank);
      }
    }
  }

  on_device_model::ModelAssetPaths model_paths;
  model_paths.sp_model = dlc_root.Append(*sp_model);
  model_paths.model = dlc_root.Append(*model_path);
  model_paths.weights = dlc_root.Append(*weight_path);

  auto params = on_device_model::mojom::LoadModelParams::New();
  params->assets = on_device_model::LoadModelAssets(model_paths);
  params->max_tokens = max_tokens.value_or(kDefaultMaxTokens);
  params->adaptation_ranks = adaptation_ranks;
  params->support_multiple_sessions = true;

  auto platform_model = base::MakeRefCounted<PlatformModel>();
  service_->LoadModel(
      std::move(params),
      platform_model->cur_model().BindNewPipeAndPassReceiver(),
      base::BindOnce(&ChromeosPlatformModelLoader::FinishLoadModel, AsWeakPtr(),
                     uuid, *version, std::move(platform_model)));
}

void ChromeosPlatformModelLoader::FinishLoadModel(
    const base::Uuid& uuid,
    const std::string& version,
    scoped_refptr<PlatformModel> model,
    mojom::LoadModelResult result) {
  if (result != mojom::LoadModelResult::kSuccess) {
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kLoadModelFail);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  CHECK(model);
  model->version() = version;
  platform_models_[uuid].platform_model = model->AsWeakPtr();

  CHECK(ReplyModelAlreadyLoaded(uuid));
  base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                LoadStatus::kFirstLoadSuccess);
}

void ChromeosPlatformModelLoader::LoadAdaptationPlatformModel(
    const base::Uuid& base_uuid,
    const std::string& base_version,
    const base::Uuid& uuid,
    const base::FilePath& dlc_root,
    const std::string& version,
    const std::string& model_path,
    const std::string& weight_path,
    scoped_refptr<PlatformModel> model,
    mojom::LoadModelResult result) {
  if (result != mojom::LoadModelResult::kSuccess) {
    LOG(ERROR) << "Failed to load base model for adaptation";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kLoadBaseModelFail);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  PlatformModelRecord& base_record = platform_models_[base_uuid];
  CHECK(base_record.platform_model);

  if (base_record.platform_model->version() != base_version) {
    LOG(ERROR) << "Base model version mismatch or no valid base model";
    base::UmaHistogramEnumeration(kLoadStatusHistogramName,
                                  LoadStatus::kInvalidModelVersion);
    ReplyError(uuid, mojom::LoadModelResult::kFailedToLoadLibrary);
    return;
  }

  on_device_model::AdaptationAssetPaths adaptation_paths;

  adaptation_paths.model = dlc_root.Append(model_path);
  adaptation_paths.weights = dlc_root.Append(weight_path);

  auto params = on_device_model::mojom::LoadAdaptationParams::New();
  params->assets = on_device_model::LoadAdaptationAssets(adaptation_paths);

  base_record.platform_model->cur_model()->LoadAdaptation(
      std::move(params), model->cur_model().BindNewPipeAndPassReceiver(),
      base::BindOnce(&ChromeosPlatformModelLoader::FinishLoadModel, AsWeakPtr(),
                     uuid, version, std::move(model)));
}

}  // namespace on_device_model
