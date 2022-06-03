// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/in_process_renderer_thread.h"

#include "build/build_config.h"
#include "content/renderer/render_process.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif

namespace content {

InProcessRendererThread::InProcessRendererThread(
    const InProcessChildThreadParams& params,
    int32_t renderer_client_id)
    : Thread("Chrome_InProcRendererThread"),
      params_(params),
      renderer_client_id_(renderer_client_id) {}

InProcessRendererThread::~InProcessRendererThread() {
  Stop();
}

void InProcessRendererThread::Init() {
  // Call AttachCurrentThreadWithName, before any other AttachCurrentThread()
  // calls. The latter causes Java VM to assign Thread-??? to the thread name.
  // Please note calls to AttachCurrentThreadWithName after AttachCurrentThread
  // will not change the thread name kept in Java VM.
#if defined(OS_ANDROID)
  base::android::AttachCurrentThreadWithName(thread_name());
  // Make sure we aren't somehow reinitialising the inprocess renderer thread on
  // Android. Temporary CHECK() to debug http://crbug.com/514141
  CHECK(!render_process_);
#endif
  blink::Platform::InitializeBlink();
  std::unique_ptr<blink::scheduler::WebThreadScheduler> main_thread_scheduler =
      blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler();

  render_process_ = RenderProcessImpl::Create();
  // RenderThreadImpl doesn't currently support a proper shutdown sequence
  // and it's okay when we're running in multi-process mode because renderers
  // get killed by the OS. In-process mode is used for test and debug only.
  new RenderThreadImpl(params_, renderer_client_id_,
                       std::move(main_thread_scheduler));
}

void InProcessRendererThread::CleanUp() {
  render_process_.reset();

  // It's a little lame to manually set this flag.  But the single process
  // RendererThread will receive the WM_QUIT.  We don't need to assert on
  // this thread, so just force the flag manually.
  // If we want to avoid this, we could create the InProcRendererThread
  // directly with _beginthreadex() rather than using the Thread class.
  // We used to set this flag in the Init function above. However there
  // other threads like WebThread which are created by this thread
  // which resets this flag. Please see Thread::StartWithOptions. Setting
  // this flag to true in Cleanup works around these problems.
  SetThreadWasQuitProperly(true);
}

base::Thread* CreateInProcessRendererThread(
    const InProcessChildThreadParams& params,
    int32_t renderer_client_id) {
  return new InProcessRendererThread(params, renderer_client_id);
}

}  // namespace content
