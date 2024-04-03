// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PLATFORM_MODEL_LOADER_CHROMEOS_H_
#define SERVICES_ON_DEVICE_MODEL_PLATFORM_MODEL_LOADER_CHROMEOS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/platform_model_loader.h"
#include "services/on_device_model/public/cpp/on_device_model.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

class ChromeosPlatformModelLoader
    : public PlatformModelLoader,
      public base::SupportsWeakPtr<ChromeosPlatformModelLoader> {
 public:
  explicit ChromeosPlatformModelLoader(mojom::OnDeviceModelService& service);
  ~ChromeosPlatformModelLoader() override;

  ChromeosPlatformModelLoader(const ChromeosPlatformModelLoader&) = delete;
  ChromeosPlatformModelLoader& operator=(const ChromeosPlatformModelLoader&) =
      delete;

  void LoadModelWithUuid(const base::Uuid& uuid,
                         mojo::PendingReceiver<mojom::OnDeviceModel> pending,
                         LoadModelCallback callback) override;

 private:
  class PlatformModel : public base::RefCounted<PlatformModel>,
                        public base::SupportsWeakPtr<PlatformModel> {
   public:
    PlatformModel();

    std::string& version() { return version_; }
    mojo::Remote<mojom::OnDeviceModel>& cur_model() { return cur_model_; }
    mojo::Remote<mojom::OnDeviceModel>& base_model() { return base_model_; }

   private:
    friend class base::RefCounted<PlatformModel>;
    virtual ~PlatformModel();

    std::string version_;
    mojo::Remote<mojom::OnDeviceModel> cur_model_;
    mojo::Remote<mojom::OnDeviceModel> base_model_;
  };

  struct PlatformModelRefTraits {
    using PointerType = scoped_refptr<PlatformModel>;
    static bool IsNull(const PointerType& ptr);
    static mojom::OnDeviceModel* GetRawPointer(PointerType* ptr);
  };

  struct PendingLoad {
    PendingLoad(mojo::PendingReceiver<mojom::OnDeviceModel> p,
                LoadModelCallback c);
    PendingLoad(PendingLoad&&);
    ~PendingLoad();

    mojo::PendingReceiver<mojom::OnDeviceModel> pending;
    LoadModelCallback callback;
  };

  struct PlatformModelRecord {
    PlatformModelRecord();
    ~PlatformModelRecord();

    base::WeakPtr<PlatformModel> platform_model;
    std::vector<PendingLoad> pending_loads;
  };

  bool ReplyModelAlreadyLoaded(const base::Uuid& uuid);

  void ReplyError(const base::Uuid& uuid, mojom::LoadModelResult result);

  void OnInstallDlcComplete(const base::Uuid& uuid,
                            const ash::DlcserviceClient::InstallResult& result);

  void LoadAdaptationPlatformModel(const base::Uuid& base_uuid,
                                   const std::string& base_version,
                                   const base::Uuid& uuid,
                                   const base::FilePath& dlc_root,
                                   const std::string& version,
                                   const std::string& model_path,
                                   const std::string& weight_path,
                                   scoped_refptr<PlatformModel> model,
                                   mojom::LoadModelResult result);

  void FinishLoadModel(const base::Uuid& uuid,
                       const std::string& version,
                       scoped_refptr<PlatformModel> model,
                       mojom::LoadModelResult result);

  raw_ref<mojom::OnDeviceModelService> service_;
  mojo::ReceiverSetBase<
      mojo::Receiver<mojom::OnDeviceModel, PlatformModelRefTraits>,
      void>
      receivers_;
  std::map<base::Uuid, PlatformModelRecord> platform_models_;
};

}  // namespace on_device_model

#endif  // SERVICES_ON_DEVICE_MODEL_PLATFORM_MODEL_LOADER_CHROMEOS_H_
