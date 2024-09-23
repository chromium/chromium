// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/in_process_gpu_thread.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/gpu/gpu_child_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/gpu/content_gpu_client.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_init.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {
namespace {

BASE_FEATURE(kInProcessGpuUseIOThread,
             "InProcessGpuUseIOThread",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

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
  base::ThreadType io_thread_type = base::ThreadType::kDefault;

  // In single-process mode, we never enter the sandbox, so run the post-sandbox
  // code now.
  content::ContentGpuClient* client = GetContentClient()->gpu();
  if (client) {
    client->PostSandboxInitialized();
  }
#if BUILDFLAG(IS_ANDROID)
  // Call AttachCurrentThreadWithName, before any other AttachCurrentThread()
  // calls. The latter causes Java VM to assign Thread-??? to the thread name.
  // Please note calls to AttachCurrentThreadWithName after AttachCurrentThread
  // will not change the thread name kept in Java VM.
  base::android::AttachCurrentThreadWithName(thread_name());
  // Up the priority of the |io_thread_| on Android.
  io_thread_type = base::ThreadType::kDisplayCritical;
#endif

  if (base::FeatureList::IsEnabled(kInProcessGpuUseIOThread)) {
    gpu_process_ = std::make_unique<ChildProcess>(params_.child_io_runner());
  } else {
    gpu_process_ = std::make_unique<ChildProcess>(io_thread_type);
  }

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
  child_thread->Init(base::TimeTicks::Now());

  gpu_process_->set_main_thread(child_thread);
}

void InProcessGpuThread::CleanUp() {
  SetThreadWasQuitProperly(true);
  gpu_process_.reset();
}

base::Thread* CreateInProcessGpuThread(
    const InProcessChildThreadParams& params,
    const gpu::GpuPreferences& gpu_preferences) {
  return new InProcessGpuThread(params, gpu_preferences);
}

}  // namespace content
