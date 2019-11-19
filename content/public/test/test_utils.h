// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_TEST_UTILS_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

#if defined(OS_ANDROID)
#include <jni.h>
#endif

namespace base {
class CommandLine;
class Value;
}  // namespace base

// A collection of functions designed for use with unit and browser tests.

namespace content {

class RenderFrameHost;
class TestServiceManagerContext;

// Create an blink::mojom::FetchAPIRequestPtr with given fields.
blink::mojom::FetchAPIRequestPtr CreateFetchAPIRequest(
    const GURL& url,
    const std::string& method,
    const blink::FetchAPIRequestHeadersMap& headers,
    blink::mojom::ReferrerPtr referrer,
    bool is_reload);

// Deprecated: Use RunLoop::Run(). Use RunLoop::Type::kNestableTasksAllowed to
// force nesting in browser tests.
void RunMessageLoop();

// Deprecated: Invoke |run_loop->Run()| directly.
void RunThisRunLoop(base::RunLoop* run_loop);

// Turns on nestable tasks, runs all pending tasks in the message loop, then
// resets nestable tasks to what they were originally. Can only be called from
// the UI thread. Only use this instead of RunLoop::RunUntilIdle() to work
// around cases where a task keeps reposting itself and prevents the loop from
// going idle.
// TODO(gab): Assess whether this API is really needed. If you find yourself
// needing this, post a comment on https://crbug.com/824431.
void RunAllPendingInMessageLoop();

// Deprecated: For BrowserThread::IO use
// BrowserTaskEnvironment::RunIOThreadUntilIdle. For the main thread use
// RunLoop. In non-unit-tests use RunLoop::QuitClosure to observe async events
// rather than flushing entire threads.
void RunAllPendingInMessageLoop(BrowserThread::ID thread_id);

// Runs all tasks on the current thread and ThreadPool threads until idle.
// Note: Prefer BrowserTaskEnvironment::RunUntilIdle() in unit tests.
void RunAllTasksUntilIdle();

// Get task to quit the given RunLoop. It allows a few generations of pending
// tasks to run as opposed to run_loop->QuitClosure().
// Prefer RunLoop::RunUntilIdle() to this.
// TODO(gab): Assess the need for this API (see comment on
// RunAllPendingInMessageLoop() above).
base::Closure GetDeferredQuitTaskForRunLoop(base::RunLoop* run_loop);

// Executes the specified JavaScript in the specified frame, and runs a nested
// MessageLoop. When the result is available, it is returned.
// This should not be used; the use of the ExecuteScript functions in
// browser_test_utils is preferable.
base::Value ExecuteScriptAndGetValue(RenderFrameHost* render_frame_host,
                                     const std::string& script);

// Returns true if all sites are isolated. Typically used to bail from a test
// that is incompatible with --site-per-process.
bool AreAllSitesIsolatedForTesting();

// Returns true if default SiteInstances are enabled. Typically used in a test
// to mark expectations specific to default SiteInstances.
bool AreDefaultSiteInstancesEnabled();

// Appends --site-per-process to the command line, enabling tests to exercise
// site isolation and cross-process iframes. This must be called early in
// the test; the flag will be read on the first real navigation.
void IsolateAllSitesForTesting(base::CommandLine* command_line);

// Resets the internal secure schemes/origins whitelist.
void ResetSchemesAndOriginsWhitelist();

// Returns a GURL constructed from the WebUI scheme and the given host.
GURL GetWebUIURL(const std::string& host);

// Returns a string constructed from the WebUI scheme and the given host.
std::string GetWebUIURLString(const std::string& host);

// Creates a WebContents and attaches it as an inner WebContents, replacing
// |rfh| in the frame tree. |rfh| should not be a main frame (in a browser test,
// it should be an <iframe>). Delegate interfaces are mocked out.
//
// Returns a pointer to the inner WebContents, which is now owned by the outer
// WebContents. The caller should be careful when retaining the pointer, as the
// inner WebContents will be deleted if the frame it's attached to goes away.
WebContents* CreateAndAttachInnerContents(RenderFrameHost* rfh);

// Helper class to Run and Quit the message loop. Run and Quit can only happen
// once per instance. Make a new instance for each use. Calling Quit after Run
// has returned is safe and has no effect.
// Note that by default Quit does not quit immediately. If that is not what you
// really need, pass QuitMode::IMMEDIATE in the constructor.
//
// DEPRECATED. Consider using base::RunLoop, in most cases MessageLoopRunner is
// not needed.  If you need to defer quitting the loop, use
// RunLoop::RunUntilIdle() and if you really think you need deferred quit (can't
// reach idle, please post details in a comment on https://crbug.com/668707).
class MessageLoopRunner : public base::RefCountedThreadSafe<MessageLoopRunner> {
 public:
  enum class QuitMode {
    // Message loop stops after finishing the current task.
    IMMEDIATE,

    // Several generations of posted tasks are executed before stopping.
    DEFERRED,
  };

  MessageLoopRunner(QuitMode mode = QuitMode::DEFERRED);

  // Run the current MessageLoop unless the quit closure
  // has already been called.
  void Run();

  // Quit the matching call to Run (nested MessageLoops are unaffected).
  void Quit();

  // Hand this closure off to code that uses callbacks to notify completion.
  // Example:
  //   scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  //   kick_off_some_api(runner->QuitClosure());
  //   runner->Run();
  base::Closure QuitClosure();

  bool loop_running() const { return loop_running_; }

 private:
  friend class base::RefCountedThreadSafe<MessageLoopRunner>;
  ~MessageLoopRunner();

  QuitMode quit_mode_;

  // True when the message loop is running.
  bool loop_running_;

  // True after closure returned by |QuitClosure| has been called.
  bool quit_closure_called_;

  base::RunLoop run_loop_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopRunner);
};

// A WindowedNotificationObserver allows code to wait until a condition is met.
// Simple conditions are specified by providing a |notification_type| and a
// |source|. When a notification of the expected type from the expected source
// is received, the condition is met.
// More complex conditions can be specified by providing a |notification_type|
// and a |callback|. The callback is called whenever the notification is fired.
// If the callback returns |true|, the condition is met. Otherwise, the
// condition is not yet met and the callback will be invoked again every time a
// notification of the expected type is received until the callback returns
// |true|. For convenience, two callback types are defined, one that is provided
// with the notification source and details, and one that is not.
//
// This helper class exists to avoid the following common pattern in tests:
//   PerformAction()
//   WaitForCompletionNotification()
// The pattern leads to flakiness as there is a window between PerformAction
// returning and the observers getting registered, where a notification will be
// missed.
//
// Rather, one can do this:
//   WindowedNotificationObserver signal(...)
//   PerformAction()
//   signal.Wait()
class WindowedNotificationObserver : public NotificationObserver {
 public:
  // Callback invoked on notifications. Should return |true| when the condition
  // being waited for is met. For convenience, there is a choice between two
  // callback types, one that is provided with the notification source and
  // details, and one that is not.
  typedef base::Callback<bool(const NotificationSource&,
                              const NotificationDetails&)>
      ConditionTestCallback;
  typedef base::Callback<bool(void)>
      ConditionTestCallbackWithoutSourceAndDetails;

  // Set up to wait for a simple condition. The condition is met when a
  // notification of the given |notification_type| from the given |source| is
  // received. To accept notifications from all sources, specify
  // NotificationService::AllSources() as |source|.
  WindowedNotificationObserver(int notification_type,
                               const NotificationSource& source);

  // Set up to wait for a complex condition. The condition is met when
  // |callback| returns |true|. The callback is invoked whenever a notification
  // of |notification_type| from any source is received.
  WindowedNotificationObserver(int notification_type,
                               const ConditionTestCallback& callback);
  WindowedNotificationObserver(
      int notification_type,
      const ConditionTestCallbackWithoutSourceAndDetails& callback);

  ~WindowedNotificationObserver() override;

  // Adds an additional notification type to wait for. The condition will be met
  // if any of the registered notification types from their respective sources
  // is received.
  void AddNotificationType(int notification_type,
                           const NotificationSource& source);

  // Wait until the specified condition is met. If the condition is already met
  // (that is, the expected notification has already been received or the
  // given callback returns |true| already), Wait() returns immediately.
  void Wait();

  // Returns NotificationService::AllSources() if we haven't observed a
  // notification yet.
  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

  // NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  bool seen_ = false;
  NotificationRegistrar registrar_;

  ConditionTestCallback callback_;

  NotificationSource source_;
  NotificationDetails details_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNotificationObserver);
};

// Unit tests can use code which runs in the utility process by having it run on
// an in-process utility thread. This eliminates having two code paths in
// production code to deal with unit tests, and also helps with the binary
// separation on Windows since chrome.dll doesn't need to call into Blink code
// for some utility code to handle the single process case.
// Include this class as a member variable in your test harness if you take
// advantage of this functionality to ensure that the in-process utility thread
// is torn down correctly. See http://crbug.com/316919 for more information.
// Note: this class should be declared after the BrowserTaskEnvironment and
// ShadowingAtExitManager (if it exists) as it will need to be run before they
// are torn down.
class InProcessUtilityThreadHelper : public BrowserChildProcessObserver {
 public:
  InProcessUtilityThreadHelper();
  ~InProcessUtilityThreadHelper() override;

 private:
  void JoinAllUtilityThreads();
  void CheckHasRunningChildProcess();
  static void CheckHasRunningChildProcessOnIO(
      const base::RepeatingClosure& quit_closure);
  void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) override;

  base::RepeatingClosure quit_closure_;
  std::unique_ptr<TestServiceManagerContext> shell_context_;

  DISALLOW_COPY_AND_ASSIGN(InProcessUtilityThreadHelper);
};

// This observer keeps track of the last deleted RenderFrame to avoid
// accessing it and causing use-after-free condition.
class RenderFrameDeletedObserver : public WebContentsObserver {
 public:
  RenderFrameDeletedObserver(RenderFrameHost* rfh);
  ~RenderFrameDeletedObserver() override;

  // Overridden WebContentsObserver methods.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

  void WaitUntilDeleted();
  bool deleted();

 private:
  int process_id_;
  int routing_id_;
  bool deleted_;
  std::unique_ptr<base::RunLoop> runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameDeletedObserver);
};

// Watches a WebContents and blocks until it is destroyed.
class WebContentsDestroyedWatcher : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(WebContents* web_contents);
  ~WebContentsDestroyedWatcher() override;

  // Waits until the WebContents is destroyed.
  void Wait();

 private:
  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedWatcher);
};

// Watches a web contents for page scales.
class TestPageScaleObserver : public WebContentsObserver {
 public:
  explicit TestPageScaleObserver(WebContents* web_contents);
  ~TestPageScaleObserver() override;
  float WaitForPageScaleUpdate();

 private:
  void OnPageScaleFactorChanged(float page_scale_factor) override;

  base::OnceClosure done_callback_;
  bool seen_page_scale_change_ = false;
  float last_scale_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(TestPageScaleObserver);
};

// A custom ContentBrowserClient that simulates GetEffectiveURL() translation
// for a single URL.
class EffectiveURLContentBrowserClient : public ContentBrowserClient {
 public:
  EffectiveURLContentBrowserClient(const GURL& url_to_modify,
                                   const GURL& url_to_return,
                                   bool requires_dedicated_process);
  ~EffectiveURLContentBrowserClient() override;

 private:
  GURL GetEffectiveURL(BrowserContext* browser_context,
                       const GURL& url) override;
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;

  GURL url_to_modify_;
  GURL url_to_return_;
  bool requires_dedicated_process_;

  DISALLOW_COPY_AND_ASSIGN(EffectiveURLContentBrowserClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_UTILS_H_
