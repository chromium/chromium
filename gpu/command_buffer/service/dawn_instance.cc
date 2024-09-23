// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_instance.h"

#include <dawn/webgpu_cpp.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "build/buildflag.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#endif

namespace gpu::webgpu {

// static
std::unique_ptr<DawnInstance> DawnInstance::Create(
    dawn::platform::Platform* platform,
    const GpuPreferences& gpu_preferences,
    SafetyLevel safety,
    WGPULoggingCallback logging_callback,
    void* logging_callback_userdata) {
  // Populate the WGSL blocklist based on the Finch feature.
  std::vector<std::string> wgsl_unsafe_features_owned;
  std::vector<const char*> wgsl_unsafe_features;

  if (safety != SafetyLevel::kUnsafe) {
    wgsl_unsafe_features_owned =
        base::SplitString(features::kWGSLUnsafeFeatures.Get(), ",",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    wgsl_unsafe_features.reserve(wgsl_unsafe_features_owned.size());
    for (const auto& f : wgsl_unsafe_features_owned) {
      wgsl_unsafe_features.push_back(f.c_str());
    }
  }
  wgpu::DawnWGSLBlocklist wgsl_blocklist;
  wgsl_blocklist.nextInChain = nullptr;
  wgsl_blocklist.blocklistedFeatureCount = wgsl_unsafe_features.size();
  wgsl_blocklist.blocklistedFeatures = wgsl_unsafe_features.data();

  // Populate the instance toggles becaused on command line parameters and
  // safety levels. Toggles which are not instance toggles will be ignored by
  // the instance.
  std::vector<const char*> require_instance_enabled_toggles;
  std::vector<const char*> require_instance_disabled_toggles;

  if (safety == SafetyLevel::kSafeExperimental) {
    require_instance_enabled_toggles.push_back(
        "expose_wgsl_experimental_features");
  } else if (safety == SafetyLevel::kUnsafe) {
    require_instance_enabled_toggles.push_back("allow_unsafe_apis");
  }

  for (const std::string& toggles :
       gpu_preferences.enabled_dawn_features_list) {
    require_instance_enabled_toggles.push_back(toggles.c_str());
  }
  for (const std::string& toggles :
       gpu_preferences.disabled_dawn_features_list) {
    require_instance_disabled_toggles.push_back(toggles.c_str());
  }

  wgpu::DawnTogglesDescriptor dawn_toggle_desc;
  dawn_toggle_desc.nextInChain = &wgsl_blocklist;
  dawn_toggle_desc.enabledToggleCount = require_instance_enabled_toggles.size();
  dawn_toggle_desc.enabledToggles = require_instance_enabled_toggles.data();
  dawn_toggle_desc.disabledToggleCount =
      require_instance_disabled_toggles.size();
  dawn_toggle_desc.disabledToggles = require_instance_disabled_toggles.data();

  // Use DawnInstanceDescriptor to pass in the platform and additional search
  // paths
  std::string dawn_search_path;
  base::FilePath module_path;
#if BUILDFLAG(IS_MAC)
  if (base::apple::AmIBundled()) {
    dawn_search_path = base::apple::FrameworkBundlePath()
                           .Append("Libraries")
                           .AsEndingWithSeparator()
                           .MaybeAsASCII();
  }
  if (dawn_search_path.empty())
#endif
  {
#if BUILDFLAG(IS_IOS)
    if (base::PathService::Get(base::DIR_ASSETS, &module_path)) {
#else
    if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
#endif
      dawn_search_path = module_path.AsEndingWithSeparator().MaybeAsASCII();
    }
  }
  const char* dawn_search_path_c_str = dawn_search_path.c_str();

  dawn::native::DawnInstanceDescriptor dawn_instance_desc;
  dawn_instance_desc.nextInChain = &dawn_toggle_desc;
  dawn_instance_desc.additionalRuntimeSearchPathsCount =
      dawn_search_path.empty() ? 0u : 1u;
  dawn_instance_desc.additionalRuntimeSearchPaths = &dawn_search_path_c_str;
  dawn_instance_desc.platform = platform;
  dawn_instance_desc.loggingCallback = logging_callback;
  dawn_instance_desc.loggingCallbackUserdata = logging_callback_userdata;

  // Create the instance with all the previous descriptors chained.
  wgpu::InstanceDescriptor instance_desc;
  instance_desc.nextInChain = &dawn_instance_desc;
  instance_desc.features.timedWaitAnyEnable = true;

  auto instance = std::make_unique<DawnInstance>(
      reinterpret_cast<const WGPUInstanceDescriptor*>(&instance_desc));

  switch (gpu_preferences.enable_dawn_backend_validation) {
    case DawnBackendValidationLevel::kDisabled:
      break;
    case DawnBackendValidationLevel::kPartial:
      instance->SetBackendValidationLevel(
          dawn::native::BackendValidationLevel::Partial);
      break;
    case DawnBackendValidationLevel::kFull:
      instance->SetBackendValidationLevel(
          dawn::native::BackendValidationLevel::Full);
      break;
  }

  return instance;
}

}  // namespace gpu::webgpu
