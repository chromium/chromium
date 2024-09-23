// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include <set>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/not_fatal_until.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/ContentViewStaticsImpl_jni.h"

using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// TODO(pliard): http://crbug.com/235909. Move WebKit shared timer toggling
// functionality out of ContentViewStatistics and not be build on top of
// blink::Platform::SuspendSharedTimer.
// TODO(pliard): http://crbug.com/235912. Add unit tests for WebKit shared timer
// toggling.

// This tracks the renderer processes that received a suspend request. It's
// important on resume to only resume the renderer processes that were actually
// suspended as opposed to all the current renderer processes because the
// suspend calls are refcounted within BlinkPlatformImpl and it expects a
// perfectly matched number of resume calls.
// Note that this class is only accessed from the UI thread.
class SuspendedProcessWatcher : public content::RenderProcessHostObserver {
 public:

  // If the process crashes, stop watching the corresponding RenderProcessHost
  // and ensure it doesn't get over-resumed.
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override {
    StopWatching(host);
  }

  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override {
    StopWatching(host);
  }

  // Suspends timers in all current render processes.
  void SuspendWebKitSharedTimers() {
    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      if (suspended_processes_.insert(host->GetID()).second) {
        host->AddObserver(this);
        host->GetRendererInterface()->SetWebKitSharedTimersSuspended(true);
      }
    }
  }

  // Resumes timers in processes that were previously stopped.
  void ResumeWebkitSharedTimers() {
    for (auto id : suspended_processes_) {
      content::RenderProcessHost* host = content::RenderProcessHost::FromID(id);
      DCHECK(host);
      host->RemoveObserver(this);
      host->GetRendererInterface()->SetWebKitSharedTimersSuspended(false);
    }
    suspended_processes_.clear();
  }

 private:
  void StopWatching(content::RenderProcessHost* host) {
    auto pos = suspended_processes_.find(host->GetID());
    CHECK(pos != suspended_processes_.end(), base::NotFatalUntil::M130);
    host->RemoveObserver(this);
    suspended_processes_.erase(pos);
  }

  std::set<int /* RenderProcessHost id */> suspended_processes_;
};

base::LazyInstance<SuspendedProcessWatcher>::DestructorAtExit
    g_suspended_processes_watcher = LAZY_INSTANCE_INITIALIZER;

}  // namespace

static void JNI_ContentViewStaticsImpl_SetWebKitSharedTimersSuspended(
    JNIEnv* env,
    jboolean suspend) {
  if (suspend) {
    g_suspended_processes_watcher.Pointer()->SuspendWebKitSharedTimers();
  } else {
    g_suspended_processes_watcher.Pointer()->ResumeWebkitSharedTimers();
  }
}
