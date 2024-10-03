// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/on_device_model_service_controller_ios.h"

#import "base/task/thread_pool.h"
#import "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "services/on_device_model/on_device_model_service.h"

namespace optimization_guide {

OnDeviceModelServiceControllerIOS::OnDeviceModelServiceControllerIOS(
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : OnDeviceModelServiceController(
          std::make_unique<OnDeviceModelAccessController>(
              *GetApplicationContext()->GetLocalState()),
          std::move(on_device_component_state_manager)) {}

OnDeviceModelServiceControllerIOS::~OnDeviceModelServiceControllerIOS() {}

void OnDeviceModelServiceControllerIOS::LaunchService() {
  receiver_ = service_remote_.BindNewPipeAndPassReceiver();
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceModelServiceControllerIOS::CreateModelService,
                     weak_factory_.GetWeakPtr()));
}

void OnDeviceModelServiceControllerIOS::CreateModelService() {
  service_ =
      on_device_model::OnDeviceModelService::Create(std::move(receiver_));
}

}  // namespace optimization_guide
