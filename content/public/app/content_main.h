// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_H_

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/stack_allocated.h"
#endif

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

class BrowserMainParts;
class ContentMainDelegate;
class ContentMainRunner;

using CreatedMainPartsClosure = base::OnceCallback<void(BrowserMainParts*)>;

struct CONTENT_EXPORT ContentMainParams {
  explicit ContentMainParams(ContentMainDelegate* delegate);
  ~ContentMainParams();

  // Do not reuse the moved-from ContentMainParams after this call.
  ContentMainParams(ContentMainParams&&);
  ContentMainParams& operator=(ContentMainParams&&);

  raw_ptr<ContentMainDelegate> delegate;

#if BUILDFLAG(IS_WIN)
  HINSTANCE instance = nullptr;

  // |sandbox_info| should be initialized using InitializeSandboxInfo from
  // content_main_win.h
  raw_ptr<sandbox::SandboxInterfaceInfo> sandbox_info = nullptr;
#elif !BUILDFLAG(IS_ANDROID)
  int argc = 0;
  raw_ptr<const char*> argv = nullptr;
#endif

  // Used by BrowserTestBase. If set, BrowserMainLoop runs this task instead of
  // the main message loop.
  base::OnceClosure ui_task;

  // Used by BrowserTestBase. If set, this is invoked after BrowserMainParts has
  // been created and before PreEarlyInitialization().
  CreatedMainPartsClosure created_main_parts_closure;

  // Indicates whether to run in a minimal browser mode where most subsystems
  // are left uninitialized.
  bool minimal_browser_mode = false;

#if BUILDFLAG(IS_MAC)
  // The outermost autorelease pool to pass to main entry points.
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  base::apple::ScopedNSAutoreleasePool* autorelease_pool = nullptr;
#endif

  // Returns a copy of this ContentMainParams without the move-only data
  // (which is expected to be null when calling this). Used by the TestLauncher
  // to launch main multiple times under the same conditions.
  ContentMainParams ShallowCopyForTesting() const {
    ContentMainParams copy(delegate);
#if BUILDFLAG(IS_WIN)
    copy.instance = instance;
    copy.sandbox_info = sandbox_info;
#elif !BUILDFLAG(IS_ANDROID)
    copy.argc = argc;
    copy.argv = argv;
#endif
    DCHECK(!ui_task);
    DCHECK(!created_main_parts_closure);
    copy.minimal_browser_mode = minimal_browser_mode;
#if BUILDFLAG(IS_MAC)
    copy.autorelease_pool = autorelease_pool;
#endif
    return copy;
  }
};

CONTENT_EXPORT int RunContentProcess(ContentMainParams params,
                                     ContentMainRunner* content_main_runner);

#if BUILDFLAG(IS_ANDROID)
// In the Android, the content main starts from ContentMain.java, This function
// provides a way to set the |delegate| as ContentMainDelegate for
// ContentMainRunner.
// This should only be called once before ContentMainRunner actually running.
// The ownership of |delegate| is transferred.
CONTENT_EXPORT void SetContentMainDelegate(ContentMainDelegate* delegate);
#else
// ContentMain should be called from the embedder's main() function to do the
// initial setup for every process. The embedder has a chance to customize
// startup using the ContentMainDelegate interface. The embedder can also pass
// in null for |delegate| if they don't want to override default startup.
CONTENT_EXPORT int ContentMain(ContentMainParams params);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_H_
