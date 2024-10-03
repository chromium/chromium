// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_ON_DEVICE_MODEL_SERVICE_CONTROLLER_IOS_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_ON_DEVICE_MODEL_SERVICE_CONTROLLER_IOS_H_

#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#import "components/optimization_guide/optimization_guide_buildflags.h"
#import "services/on_device_model/on_device_model_service.h"

#if !BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#error "This controller shouldn't be compiled without the flag."
#endif  // !BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

namespace optimization_guide {
class OnDeviceModelComponentStateManager;
// Chrome uses a single instance of OnDeviceModelServiceController. This is done
// for two reasons:
// . We only want to load the model once, not once per profile. To do otherwise
//   would consume a significant amount of memory.
// . To prevent the number of crashes from being multiplied (if each profile had
//   it's own connection, then a crash would be logged for each one).
class OnDeviceModelServiceControllerIOS
    : public OnDeviceModelServiceController {
 public:
  explicit OnDeviceModelServiceControllerIOS(
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

 private:
  ~OnDeviceModelServiceControllerIOS() override;

  // Launches the on-device model service.
  void LaunchService() override;

  // Creates the on-device model service.
  void CreateModelService();

  // The on-device model service.
  std::unique_ptr<on_device_model::mojom::OnDeviceModelService> service_;

  // Receives interface messages.
  mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService> receiver_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<OnDeviceModelServiceControllerIOS> weak_factory_{this};
};

}  // namespace optimization_guide

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_ON_DEVICE_MODEL_SERVICE_CONTROLLER_IOS_H_
