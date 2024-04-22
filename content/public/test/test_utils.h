// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_TEST_UTILS_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/fetch/fetch_api_request_headers_map.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom-forward.h"

#if BUILDFLAG(IS_ANDROID)
#include <jni.h>
#endif

namespace base {
class CommandLine;
}  // namespace base

// A collection of functions designed for use with unit and browser tests.

namespace content {

class RenderFrameHost;

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
base::OnceClosure GetDeferredQuitTaskForRunLoop(base::RunLoop* run_loop);

// Returns true if all sites are isolated. Typically used to bail from a test
// that is incompatible with --site-per-process.
bool AreAllSitesIsolatedForTesting();

// Returns true if |origin| is currently isolated with respect to the
// BrowsingInstance of |site_instance|. This is only relevant for
// OriginAgentCluster isolation, and not other types of origin isolation.
// Note: this only indicates logcial OriginAgentCluster isolation, and says
// nothing about process-isolation (RequiresOriginKeyedProcess).
bool IsOriginAgentClusterEnabledForOrigin(SiteInstance* site_instance,
                                          const url::Origin& origin);

// Returns true if default SiteInstances are enabled. Typically used in a test
// to mark expectations specific to default SiteInstances.
bool AreDefaultSiteInstancesEnabled();

// Returns true if the process model only allows a SiteInstance to contain
// a single site.
bool AreStrictSiteInstancesEnabled();

// Returns true if a test needs to register an origin for isolation to ensure
// that navigations, for that origin, are placed in a dedicated process. Some
// process model modes allow sites to share a process if they are not isolated.
// This helper indicates when such a mode is in use and indicates the test must
// register an isolated origin to ensure the origin gets placed in its own
// process.
bool IsIsolatedOriginRequiredToGuaranteeDedicatedProcess();

// Appends --site-per-process to the command line, enabling tests to exercise
// site isolation and cross-process iframes. This must be called early in
// the test; the flag will be read on the first real navigation.
void IsolateAllSitesForTesting(base::CommandLine* command_line);

// Whether same-site navigations might result in a change of RenderFrameHosts -
// this will happen when ProactivelySwapBrowsingInstance, RenderDocument or
// back-forward cache is enabled on same-site main frame navigations. Note that
// even if this returns true, not all same-site main frame navigations will
// result in a change of RenderFrameHosts, e.g. if RenderDocument is disabled
// but BFCache is enabled, this will return true but only same-site navigations
// from pages that are BFCache-eligible will result in a RenderFrameHost change.
bool CanSameSiteMainFrameNavigationsChangeRenderFrameHosts();

// Whether same-site navigations will result in a change of RenderFrameHosts,
// which will happen when RenderDocument is enabled. Due to the various levels
// of the feature, the result may differ depending on whether the
// RenderFrameHost is a main/local root/non-local-root frame.
bool WillSameSiteNavigationChangeRenderFrameHosts(bool is_main_frame,
                                                  bool is_local_root = true);

// Whether same-site navigations might result in a change of SiteInstances -
// this will happen when ProactivelySwapBrowsingInstance or back-forward cache
// is enabled on same-site main frame navigations.
// Note that unlike WillSameSiteNavigationChangeRenderFrameHosts()
// above, this will not be true when RenderDocument for main-frame is enabled.
bool CanSameSiteMainFrameNavigationsChangeSiteInstances();

// Returns true if navigation queueing is fully enabled, where we will queue new
// navigations that happen when there is an existing pending commit navigation.
bool IsNavigationQueueingEnabled();

// Makes sure that navigations that start in |rfh| won't result in a proactive
// BrowsingInstance swap (note they might still result in a normal
// BrowsingInstance swap, e.g. in the case of cross-site navigations).
void DisableProactiveBrowsingInstanceSwapFor(RenderFrameHost* rfh);

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

// Spins a run loop until IsDocumentOnLoadCompletedInPrimaryMainFrame() is true.
void AwaitDocumentOnLoadCompleted(WebContents* web_contents);

// Sets the focused frame of `web_contents` to the `rfh` for tests that rely on
// the focused frame not being null.
void FocusWebContentsOnFrame(WebContents* web_contents, RenderFrameHost* rfh);

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

  explicit MessageLoopRunner(QuitMode mode = QuitMode::DEFERRED);

  MessageLoopRunner(const MessageLoopRunner&) = delete;
  MessageLoopRunner& operator=(const MessageLoopRunner&) = delete;

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
  base::OnceClosure QuitClosure();

  bool loop_running() const { return loop_running_; }

 private:
  friend class base::RefCountedThreadSafe<MessageLoopRunner>;
  ~MessageLoopRunner();

  QuitMode quit_mode_;

  // True when the message loop is running.
  bool loop_running_ = false;

  // True after closure returned by |QuitClosure| has been called.
  bool quit_closure_called_ = false;

  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};

  base::ThreadChecker thread_checker_;
};

// Helper to wait for loading to stop on a WebContents.
//
// This helper class exists to avoid the following common pattern in tests:
//   PerformAction()
//   WaitForCompletionNotification()
// The pattern leads to flakiness as there is a window between PerformAction
// returning and the observers getting registered, where a notification will be
// missed.
//
// Rather, one can do this:
//   LoadStopObserver signal(web_contents)
//   PerformAction()
//   signal.Wait()
class LoadStopObserver : public WebContentsObserver {
 public:
  explicit LoadStopObserver(WebContents* web_contents);

  // Wait until at least one load stop has been observed.  Return immediately if
  // one has been observed since construction.
  void Wait();

  // WebContentsObserver
  void DidStopLoading() override;

 private:
  bool seen_ = false;
  base::RunLoop run_loop_;
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

  InProcessUtilityThreadHelper(const InProcessUtilityThreadHelper&) = delete;
  InProcessUtilityThreadHelper& operator=(const InProcessUtilityThreadHelper&) =
      delete;

  ~InProcessUtilityThreadHelper() override;

 private:
  void JoinAllUtilityThreads();
  void CheckHasRunningChildProcess();
  void BrowserChildProcessHostDisconnected(
      const ChildProcessData& data) override;

  std::optional<base::RunLoop> run_loop_;
};

// This observer keeps tracks of whether a given RenderFrameHost has received
// WebContentsObserver::RenderFrameDeleted.
class RenderFrameDeletedObserver : public WebContentsObserver {
 public:
  // |rfh| should not already be deleted.
  explicit RenderFrameDeletedObserver(RenderFrameHost* rfh);

  RenderFrameDeletedObserver(const RenderFrameDeletedObserver&) = delete;
  RenderFrameDeletedObserver& operator=(const RenderFrameDeletedObserver&) =
      delete;

  ~RenderFrameDeletedObserver() override;

  // Overridden WebContentsObserver methods.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;

  // TODO(crbug.com/40204325): Add [[nodiscard]]
  // Returns true if the frame was deleted before the timeout.
  bool WaitUntilDeleted();
  bool deleted() const;

 private:
  // We cannot keep a pointer because if the RenderFrameHost is not in the
  // created state when this class is initialized, then RenderFrameDeleted might
  // not be called when it is destroyed.
  GlobalRenderFrameHostId rfh_id_;
  std::unique_ptr<base::RunLoop> runner_;
};

// This class holds a RenderFrameHost*, providing safe access to it for testing.
// If the RFH is destroyed, it can no longer be accessed. Attempting to access
// it via dereference will cause a DCHECK failure.
//
// For convenience, it also wraps a RenderFrameDeletedObserver and provides
// access to |deleted| and |WaitForDeleted|. Note, deletion of the RenderFrame
// does not always correspond to destruction of the RenderFrameHost, see
// the comments on |RenderFrameDeletedObserver|).
class RenderFrameHostWrapper {
 public:
  explicit RenderFrameHostWrapper(RenderFrameHost* rfh);
  ~RenderFrameHostWrapper();
  RenderFrameHostWrapper(RenderFrameHostWrapper&&);

  // Returns the pointer or nullptr if the RFH has already been destroyed.
  RenderFrameHost* get() const;
  // Returns true if RenderFrameHost has been destroyed.
  bool IsDestroyed() const;

  // See RenderFrameDeletedObserver for notes on the difference between
  // RenderFrame being deleted and RenderFrameHost being destroyed.
  // Returns true if the frame was deleted before the timeout.
  [[nodiscard]] bool WaitUntilRenderFrameDeleted() const;
  bool IsRenderFrameDeleted() const;

  // Pointerish operators. Feel free to add more if you need them.
  RenderFrameHost& operator*() const;
  RenderFrameHost* operator->() const;

  explicit operator bool() const { return get() != nullptr; }

 private:
  const GlobalRenderFrameHostId rfh_id_;

  // It's tempting to just inherit but RenderFrameDeletedObserver is not
  // movable because it is a WebContentsObserver.
  std::unique_ptr<RenderFrameDeletedObserver> deleted_observer_;
};

// Watches a WebContents. Can be used to block until it is destroyed or just
// merely report if it was destroyed.
class WebContentsDestroyedWatcher : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(WebContents* web_contents);

  WebContentsDestroyedWatcher(const WebContentsDestroyedWatcher&) = delete;
  WebContentsDestroyedWatcher& operator=(const WebContentsDestroyedWatcher&) =
      delete;

  ~WebContentsDestroyedWatcher() override;

  // Waits until the WebContents is destroyed.
  void Wait();

  // Returns whether the WebContents was destroyed.
  bool IsDestroyed() { return destroyed_; }

 private:
  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override;

  base::RunLoop run_loop_;

  bool destroyed_ = false;
};

// Watches a web contents for page scales.
class TestPageScaleObserver : public WebContentsObserver {
 public:
  explicit TestPageScaleObserver(WebContents* web_contents);

  TestPageScaleObserver(const TestPageScaleObserver&) = delete;
  TestPageScaleObserver& operator=(const TestPageScaleObserver&) = delete;

  ~TestPageScaleObserver() override;
  float WaitForPageScaleUpdate();

 private:
  void OnPageScaleFactorChanged(float page_scale_factor) override;

  base::OnceClosure done_callback_;
  bool seen_page_scale_change_ = false;
  float last_scale_ = 0.f;
};

class EffectiveURLContentBrowserClientHelper {
 public:
  explicit EffectiveURLContentBrowserClientHelper(
      bool requires_dedicated_process = false);
  ~EffectiveURLContentBrowserClientHelper();

  void AddTranslation(const GURL& url_to_modify, const GURL& url_to_return);
  GURL GetEffectiveURL(const GURL& url);
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url);

 private:
  // A map of original URLs to effective URLs.
  std::map<GURL, GURL> urls_to_modify_;

  const bool requires_dedicated_process_;
};

// A custom ContentBrowserClient that simulates GetEffectiveURL() translation
// for one or more URL pairs.  |requires_dedicated_process| indicates whether
// the client should indicate that each registered URL requires a dedicated
// process.  Passing |false| for it will rely on default behavior computed in
// SiteInstanceImpl::DoesSiteRequireDedicatedProcess().
//
// Do not use this in browser tests. Instead use
// EffectiveURLContentBrowserTestContentBrowserClient.
class EffectiveURLContentBrowserClient : public ContentBrowserClient {
 public:
  explicit EffectiveURLContentBrowserClient(bool requires_dedicated_process);
  EffectiveURLContentBrowserClient(const GURL& url_to_modify,
                                   const GURL& url_to_return,
                                   bool requires_dedicated_process);

  EffectiveURLContentBrowserClient(const EffectiveURLContentBrowserClient&) =
      delete;
  EffectiveURLContentBrowserClient& operator=(
      const EffectiveURLContentBrowserClient&) = delete;

  ~EffectiveURLContentBrowserClient() override;

  // Adds effective URL translation from |url_to_modify| to |url_to_return|.
  void AddTranslation(const GURL& url_to_modify, const GURL& url_to_return);

 private:
  GURL GetEffectiveURL(BrowserContext* browser_context,
                       const GURL& url) override;
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;

  EffectiveURLContentBrowserClientHelper helper_;
};

// Wrapper around `SetBrowserClientForTesting()` that ensures the
// previous content browser client is restored upon destruction. This is
// unnecessary in browser tests. In browser tests subclass
// ContentBrowserTestContentBrowserClient and it will take care of this for you.
class ScopedContentBrowserClientSetting final {
 public:
  explicit ScopedContentBrowserClientSetting(ContentBrowserClient* new_client);
  ~ScopedContentBrowserClientSetting();

  ScopedContentBrowserClientSetting(const ScopedContentBrowserClientSetting&) =
      delete;
  ScopedContentBrowserClientSetting(ScopedContentBrowserClientSetting&&) =
      delete;

  ScopedContentBrowserClientSetting& operator=(
      const ScopedContentBrowserClientSetting&) = delete;
  ScopedContentBrowserClientSetting& operator=(
      ScopedContentBrowserClientSetting&&) = delete;

 private:
  const raw_ptr<ContentBrowserClient> old_client_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_UTILS_H_
