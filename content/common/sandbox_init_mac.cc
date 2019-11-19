// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandbox_init.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_switching.h"
#include "gpu/config/gpu_util.h"
#include "media/gpu/mac/vt_video_decode_accelerator_mac.h"
#include "sandbox/mac/seatbelt.h"
#include "sandbox/mac/seatbelt_exec.h"
#include "services/service_manager/sandbox/mac/sandbox_mac.h"
#include "services/service_manager/sandbox/sandbox.h"
#include "services/service_manager/sandbox/sandbox_type.h"
#include "ui/gl/init/gl_factory.h"

namespace content {

namespace {

// Helper method to make a closure from a closure.
base::OnceClosure MaybeWrapWithGPUSandboxHook(
    service_manager::SandboxType sandbox_type,
    base::OnceClosure original) {
  if (sandbox_type != service_manager::SANDBOX_TYPE_GPU)
    return original;

  return base::BindOnce(
      [](base::OnceClosure arg) {
        // We need to gather GPUInfo and compute GpuFeatureInfo here, so we can
        // decide if initializing core profile or compatibility profile GL,
        // depending on gpu driver bug workarounds.
        gpu::GPUInfo gpu_info;
        auto* command_line = base::CommandLine::ForCurrentProcess();
        gpu::CollectBasicGraphicsInfo(command_line, &gpu_info);
        gpu::CacheGPUInfo(gpu_info);
        gpu::GpuPreferences gpu_preferences;
        if (command_line->HasSwitch(switches::kGpuPreferences)) {
          std::string value =
              command_line->GetSwitchValueASCII(switches::kGpuPreferences);
          bool success = gpu_preferences.FromSwitchValue(value);
          CHECK(success);
        }
        bool needs_more_info = false;
        gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfo(
            gpu_info, gpu_preferences, command_line, &needs_more_info);
        gpu::CacheGpuFeatureInfo(gpu_feature_info);
        if (gpu::SwitchableGPUsSupported(gpu_info, *command_line)) {
          gpu::InitializeSwitchableGPUs(
              gpu_feature_info.enabled_gpu_driver_bug_workarounds);
        }
        gpu::EnableSwiftShaderIfNeeded(
            command_line, gpu_feature_info,
            gpu_preferences.disable_software_rasterizer, needs_more_info);
        // Preload either the desktop GL or SwiftShader so, depending on the
        // --use-gl flag.
        gl::init::InitializeGLOneOff();

        // Preload VideoToolbox.
        media::InitializeVideoToolbox();

        // Invoke original hook.
        if (!arg.is_null())
          std::move(arg).Run();
      },
      std::move(original));
}

// Fill in |sandbox_type| based on the command line.  Returns false if the
// current process type doesn't need to be sandboxed or if the sandbox was
// disabled from the command line.
bool GetSandboxTypeFromCommandLine(service_manager::SandboxType* sandbox_type) {
  DCHECK(sandbox_type);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  *sandbox_type = service_manager::SandboxTypeFromCommandLine(*command_line);
  if (service_manager::IsUnsandboxedSandboxType(*sandbox_type))
    return false;

  if (command_line->HasSwitch(sandbox::switches::kSeatbeltClientName)) {
    CHECK(sandbox::Seatbelt::IsSandboxed());
    // Do not enable the sandbox if V2 is already enabled.
    return false;
  }

  return *sandbox_type != service_manager::SANDBOX_TYPE_INVALID;
}

}  // namespace

bool InitializeSandbox(service_manager::SandboxType sandbox_type) {
  return service_manager::Sandbox::Initialize(
      sandbox_type,
      MaybeWrapWithGPUSandboxHook(sandbox_type, base::OnceClosure()));
}

bool InitializeSandbox(base::OnceClosure post_warmup_hook) {
  service_manager::SandboxType sandbox_type =
      service_manager::SANDBOX_TYPE_INVALID;
  return !GetSandboxTypeFromCommandLine(&sandbox_type) ||
         service_manager::Sandbox::Initialize(
             sandbox_type, MaybeWrapWithGPUSandboxHook(
                               sandbox_type, std::move(post_warmup_hook)));
}

bool InitializeSandbox() {
  return InitializeSandbox(base::OnceClosure());
}

}  // namespace content
