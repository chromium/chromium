// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
#define CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/main_function_params.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace variations {
class VariationsIdsProvider;
}

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentGpuClient;
class ContentRendererClient;
class ContentUtilityClient;
class ZygoteForkDelegate;

class CONTENT_EXPORT ContentMainDelegate {
 public:
  // Indicates the delegate is being invoked in the browser process. The
  // `kProcessType` switch will be empty.
  struct InvokedInBrowserProcess {
    // True if running in a test harness.
    bool is_running_test = false;
  };

  // Indicates the delegate is being invoked in a child process. The
  // `kProcessType` switch will hold the precise child process type.
  struct InvokedInChildProcess {
    // True if the child process was forked from one of the browser's zygotes.
    bool is_zygote_child = false;
  };

  // The context in which a delegate method is invoked, including the process
  // type and whether it is in a test harness. Can distinguish between
  // the browser process and child processes; for more fine-grained process
  // types check the `switches::kProcessType` command-line switch.
  using InvokedIn =
      absl::variant<InvokedInBrowserProcess, InvokedInChildProcess>;

  virtual ~ContentMainDelegate() = default;

  // Tells the embedder that the absolute basic startup has been done, i.e.
  // it's now safe to create singletons and check the command line. Return an
  // error code if the process should exit afterwards. This is the place for
  // embedder to do the things that must happen at the start. Most of its
  // startup code should be in the methods below, handling of early exit
  // command-line switches can wait until PreBrowserMain at the latest.
  virtual std::optional<int> BasicStartupComplete();

  // This is where the embedder puts all of its startup code that needs to run
  // before the sandbox is engaged.
  virtual void PreSandboxStartup() {}

  // This is where the embedder can add startup code to run after the sandbox
  // has been initialized.
  virtual void SandboxInitialized(const std::string& process_type) {}

  // Asks the embedder to start a process. The embedder may return the
  // |main_function_params| back to decline the request and kick-off the
  // default behavior or return a non-negative exit code to indicate it handled
  // the request.
  virtual absl::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params);

  // Called right before the process exits. Note: an empty process_type must not
  // be assumed to be an exclusive browser process, processes that exit early
  // (e.g. attempt and fail to be the browser process) will also go through this
  // path.
  virtual void ProcessExiting(const std::string& process_type) {}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Tells the embedder that the zygote process is starting, and allows it to
  // specify one or more zygote delegates if it wishes by storing them in
  // |*delegates|.
  virtual void ZygoteStarting(
      std::vector<std::unique_ptr<ZygoteForkDelegate>>* delegates);

  // Called every time the zygote process forks.
  virtual void ZygoteForked() {}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  // Fatal errors during initialization are reported by this function, so that
  // the embedder can implement graceful exit by displaying some message and
  // returning initialization error code. Default behavior is CHECK(false).
  virtual int TerminateForFatalInitializationError();

  // Allows the embedder to prevent locking the scheme registry. The scheme
  // registry is the list of URL schemes we recognize, with some additional
  // information about each scheme such as whether it expects a host. The
  // scheme registry is not thread-safe, so by default it is locked before any
  // threads are created to ensure single-threaded access. An embedder can
  // override this to prevent the scheme registry from being locked during
  // startup, but if they do so then they are responsible for making sure that
  // the registry is only accessed in a thread-safe way, and for calling
  // url::LockSchemeRegistries() when initialization is complete. If possible,
  // prefer registering additional schemes through
  // ContentClient::AddAdditionalSchemes over preventing the scheme registry
  // from being locked.
  virtual bool ShouldLockSchemeRegistry();

  // Allows the embedder to perform platform-specific initialization before
  // BrowserMain() is invoked (i.e. before BrowserMainRunner, BrowserMainLoop,
  // BrowserMainParts, etc. are created). Return an error code if the process
  // should exit afterwards. This is the place for embedder to do the things
  // that can shortcut browser execution (i.e. command-line switches).
  virtual std::optional<int> PreBrowserMain();

  // Returns true if content should create field trials and initialize the
  // FeatureList instance for this process. Default implementation returns true.
  // Embedders that need to control when and/or how FeatureList should be
  // created should override and return false.
  virtual bool ShouldCreateFeatureList(InvokedIn invoked_in);

  // Returns true if content should initialize Mojo before calling
  // PostEarlyInitialization(). Returns true by default. If this returns false,
  // the embedder must initialize Mojo. Embedders may wish to override this to
  // control when Mojo is initialized; for example, Mojo needs to be initialized
  // after FeatureList, so embedders who delay FeatureList setup must also delay
  // Mojo setup.
  virtual bool ShouldInitializeMojo(InvokedIn invoked_in);

  // Creates and returns the VariationsIdsProvider. If null is returned,
  // a VariationsIdsProvider is created with a mode of `kUseSignedInState`.
  // VariationsIdsProvider is a singleton.
  virtual variations::VariationsIdsProvider* CreateVariationsIdsProvider();

  // Called when it's time to create a base::ThreadPoolInstance for the
  // browser process. This is not exposed in ContentBrowserClient
  // because it needs to happen before ContentBrowserClient is created.
  //
  // Note: The embedder must *not* start the created ThreadPoolInstance. That
  // will be done by //content when appropriate.
  virtual void CreateThreadPool(std::string_view name);

  // Allows the embedder to perform its own initialization after early content
  // initialization.
  //
  // At this point, it is possible to post to base::ThreadPool, but the tasks
  // won't run until base::ThreadPoolInstance::Start() is called.
  //
  // It is also possible to post tasks to the main thread loop via
  // base::SingleThreadTaskRunner::CurrentDefaultHandle. These tasks won't run
  // until base::RunLoop::Run() is called on the main thread, which happens
  // after all ContentMainDelegate entry points.
  //
  // If ShouldCreateFeatureList() returns true for `invoked_in`, the
  // field trials and FeatureList have been initialized. Otherwise, the
  // implementation must initialize the field trials and FeatureList before
  // returning from PostEarlyInitialization. Return an error code if the process
  // should exit afterwards.
  virtual std::optional<int> PostEarlyInitialization(InvokedIn invoked_in);

#if BUILDFLAG(IS_WIN)
  // Allows the embedder to indicate that console control events (e.g., Ctrl-C,
  // Ctrl-break, or closure of the console) are to be handled. By default, these
  // events are not handled, leading to process termination. When an embedder
  // returns true to indicate that these events are to be handled, the
  // embedder's ContentBrowserClient::SessionEnding function will be called
  // when a console control event is received. All non-browser processes will
  // swallow the event.
  virtual bool ShouldHandleConsoleControlEvents();
#endif

 protected:
  friend class ContentClientCreator;
  friend class ContentClientInitializer;
  friend class BrowserTestBase;

  // Called once per relevant process type to allow the embedder to customize
  // content. If an embedder wants the default (empty) implementation, don't
  // override this.
  virtual ContentClient* CreateContentClient();
  virtual ContentBrowserClient* CreateContentBrowserClient();
  virtual ContentGpuClient* CreateContentGpuClient();
  virtual ContentRendererClient* CreateContentRendererClient();
  virtual ContentUtilityClient* CreateContentUtilityClient();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_MAIN_DELEGATE_H_
