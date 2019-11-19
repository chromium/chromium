// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_H_

#include <stddef.h>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {
namespace mac {
class ScopedNSAutoreleasePool;
}
}

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

class BrowserMainParts;
class ContentMainDelegate;

using CreatedMainPartsClosure = base::Callback<void(BrowserMainParts*)>;

struct ContentMainParams {
  explicit ContentMainParams(ContentMainDelegate* delegate)
      : delegate(delegate) {}

  ContentMainDelegate* delegate;

#if defined(OS_WIN)
  HINSTANCE instance = nullptr;

  // |sandbox_info| should be initialized using InitializeSandboxInfo from
  // content_main_win.h
  sandbox::SandboxInterfaceInfo* sandbox_info = nullptr;
#elif !defined(OS_ANDROID)
  int argc = 0;
  const char** argv = nullptr;
#endif

  // Used by browser_tests. If non-null BrowserMain schedules this task to run
  // on the MessageLoop. It's owned by the test code.
  base::Closure* ui_task = nullptr;

  // Used by InProcessBrowserTest. If non-null this is Run() after
  // BrowserMainParts has been created and before PreEarlyInitialization().
  CreatedMainPartsClosure* created_main_parts_closure = nullptr;

#if defined(OS_MACOSX)
  // The outermost autorelease pool to pass to main entry points.
  base::mac::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#endif
};

#if defined(OS_ANDROID)
// In the Android, the content main starts from ContentMain.java, This function
// provides a way to set the |delegate| as ContentMainDelegate for
// ContentMainRunner.
// This should only be called once before ContentMainRunner actually running.
// The ownership of |delegate| is transferred.
CONTENT_EXPORT void SetContentMainDelegate(ContentMainDelegate* delegate);

// In browser tests, ContentMain.java is not run either, and the browser test
// harness does not run ContentMain() at all. It does need to make use of the
// delegate though while replacing ContentMain().
CONTENT_EXPORT ContentMainDelegate* GetContentMainDelegateForTesting();
#else
// ContentMain should be called from the embedder's main() function to do the
// initial setup for every process. The embedder has a chance to customize
// startup using the ContentMainDelegate interface. The embedder can also pass
// in NULL for |delegate| if they don't want to override default startup.
CONTENT_EXPORT int ContentMain(const ContentMainParams& params);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
