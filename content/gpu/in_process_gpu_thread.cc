// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/in_process_gpu_thread.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/gpu/gpu_process.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_init.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "content/common/android/cpu_affinity_setter.h"
#endif

namespace content {

InProcessGpuThread::InProcessGpuThread(
    const InProcessChildThreadParams& params,
    const gpu::GpuPreferences& gpu_preferences)
    : base::Thread("Chrome_InProcGpuThread"),
      params_(params),
      gpu_process_(nullptr),
      gpu_preferences_(gpu_preferences) {}

InProcessGpuThread::~InProcessGpuThread() {
  Stop();
}

void InProcessGpuThread::Init() {
  base::ThreadPriority io_thread_priority = base::ThreadPriority::NORMAL;

#if BUILDFLAG(IS_ANDROID)
  // Call AttachCurrentThreadWithName, before any other AttachCurrentThread()
  // calls. The latter causes Java VM to assign Thread-??? to the thread name.
  // Please note calls to AttachCurrentThreadWithName after AttachCurrentThread
  // will not change the thread name kept in Java VM.
  base::android::AttachCurrentThreadWithName(thread_name());
  // Up the priority of the |io_thread_| on Android.
  io_thread_priority = base::ThreadPriority::DISPLAY;

  if (base::GetFieldTrialParamByFeatureAsBool(
          features::kBigLittleScheduling,
          features::kBigLittleSchedulingGpuMainBigParam, false)) {
    SetCpuAffinityForCurrentThread(base::CpuAffinityMode::kBigCoresOnly);
  }
#endif

  gpu_process_ = new GpuProcess(io_thread_priority);

  auto gpu_init = std::make_unique<gpu::GpuInit>();
  gpu_init->InitializeInProcess(base::CommandLine::ForCurrentProcess(),
                                gpu_preferences_);

#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  GetContentClient()->SetGpuInfo(gpu_init->gpu_info());

  // The process object takes ownership of the thread object, so do not
  // save and delete the pointer.
  GpuChildThread* child_thread =
      new GpuChildThread(params_, std::move(gpu_init));

  // Since we are in the browser process, use the thread start time as the
  // process start time.
  child_thread->Init(base::Time::Now());

  gpu_process_->set_main_thread(child_thread);
}

void InProcessGpuThread::CleanUp() {
  SetThreadWasQuitProperly(true);
  delete gpu_process_;
}

base::Thread* CreateInProcessGpuThread(
    const InProcessChildThreadParams& params,
    const gpu::GpuPreferences& gpu_preferences) {
  return new InProcessGpuThread(params, gpu_preferences);
}

}  // namespace content
