// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_global_state.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/path_service.h"
#import "base/system/sys_info.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/component_updater/component_updater_paths.h"
#import "components/optimization_guide/core/delivery/prediction_manager.h"
#import "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#import "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/services/unzip/in_process_unzipper.h"
#import "ios/chrome/browser/optimization_guide/model/chrome_profile_download_service_tracker.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

OptimizationGuideGlobalState::OptimizationGuideGlobalState()
    : prediction_model_store_(*GetApplicationContext()->GetLocalState()),
      prediction_manager_(
          &prediction_model_store_,
          GetApplicationContext()->GetSharedURLLoaderFactory(),
          GetApplicationContext()->GetLocalState(),
          GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
          OptimizationGuideLogger::GetInstance(),
          base::BindRepeating(&unzip::LaunchInProcessUnzipper)) {
  prediction_manager_.MaybeInitializeModelDownloads(
      profile_download_service_tracker_,
      GetApplicationContext()->GetLocalState());
}

OptimizationGuideGlobalState::~OptimizationGuideGlobalState() = default;

}  // namespace optimization_guide
