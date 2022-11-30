// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ml/ml_service_impl_cros.h"

#include "base/memory/ptr_util.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/ml/mojom/ml_service.mojom.h"
#include "components/ml/mojom/web_platform_model.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

using ml::model_loader::mojom::CreateModelLoaderOptionsPtr;
using ml::model_loader::mojom::CreateModelLoaderResult;
using ml::model_loader::mojom::MLService;
using ml::model_loader::mojom::ModelLoader;

void OnModelCreated(mojo::PendingRemote<ModelLoader> remote,
                    MLService::CreateModelLoaderCallback callback,
                    CreateModelLoaderResult result) {
  std::move(callback).Run(result, std::move(remote));
}

}  // namespace

// static
void CrOSMLServiceImpl::Create(mojo::PendingReceiver<MLService> receiver) {
  mojo::MakeSelfOwnedReceiver<MLService>(
      base::WrapUnique(new CrOSMLServiceImpl()), std::move(receiver));
}

CrOSMLServiceImpl::~CrOSMLServiceImpl() = default;

CrOSMLServiceImpl::CrOSMLServiceImpl() = default;

void CrOSMLServiceImpl::CreateModelLoader(
    CreateModelLoaderOptionsPtr options,
    MLService::CreateModelLoaderCallback callback) {
  // The remote sent to the renderer.
  mojo::PendingRemote<ModelLoader> blink_remote;
  // The receiver sent to ml-service.
  auto cros_receiver = blink_remote.InitWithNewPipeAndPassReceiver();

  // TODO(https://crbug.com/1309814): we should consider restricting the
  // resource usage of this API.
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .CreateWebPlatformModelLoader(
          std::move(cros_receiver), std::move(options),
          base::BindOnce(&OnModelCreated, std::move(blink_remote),
                         std::move(callback)));
}

}  // namespace content
