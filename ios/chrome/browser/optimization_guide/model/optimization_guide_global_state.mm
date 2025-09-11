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
#import "components/component_updater/component_updater_paths.h"
#import "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#import "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

OptimizationGuideGlobalState::OptimizationGuideGlobalState()
    : prediction_model_store_(*GetApplicationContext()->GetLocalState()) {}

OptimizationGuideGlobalState::~OptimizationGuideGlobalState() = default;

}  // namespace optimization_guide
