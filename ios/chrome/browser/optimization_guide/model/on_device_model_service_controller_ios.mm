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

namespace {

// Creates the on-device model service.
void CreateModelService(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        receiver) {
  // TODO(crbug.com/370768381): Save the pointer of OnDeviceModelService. Otherwise, it will
  // disconnect the service immediately.
  on_device_model::OnDeviceModelService::Create(std::move(receiver));
}

// Launches the on-device model service.
void LaunchService(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        pending_receiver) {
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  background_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateModelService, std::move(pending_receiver)));
}

}  // namespace

OnDeviceModelServiceControllerIOS::OnDeviceModelServiceControllerIOS(
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : OnDeviceModelServiceController(
          std::make_unique<OnDeviceModelAccessController>(
              *GetApplicationContext()->GetLocalState()),
          std::move(on_device_component_state_manager),
          base::BindRepeating(&LaunchService)) {}

OnDeviceModelServiceControllerIOS::~OnDeviceModelServiceControllerIOS() {}

}  // namespace optimization_guide
