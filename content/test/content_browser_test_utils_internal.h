// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_

// A collection of functions designed for use with content_shell based browser
// tests internal to the content/ module.
// Note: If a function here also works with browser_tests, it should be in
// the content public API.

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"
#include "third_party/blink/public/mojom/choosers/popup_menu.mojom.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class RenderFrameHost;
class RenderFrameHostImpl;
class RenderWidgetHostImpl;
class Shell;
class SiteInstance;
class ToRenderFrameHost;

// Navigates the frame represented by |node| to |url|, blocking until the
// navigation finishes. Returns true if the navigation succeedd and the final
// URL matches |url|.
bool NavigateFrameToURL(FrameTreeNode* node, const GURL& url);

// Sets the DialogManager to proceed by default or not when showing a
// BeforeUnload dialog, and if it proceeds, what value to return.
void SetShouldProceedOnBeforeUnload(Shell* shell, bool proceed, bool success);

// Extends the ToRenderFrameHost mechanism to FrameTreeNodes.
RenderFrameHost* ConvertToRenderFrameHost(FrameTreeNode* frame_tree_node);

// Helper function to navigate a window to a |url|, using a browser-initiated
// navigation that will stay in the same BrowsingInstance.  Most
// browser-initiated navigations swap BrowsingInstances, but some tests need a
// navigation to swap processes for cross-site URLs (even outside of
// --site-per-process) while staying in the same BrowsingInstance.
WARN_UNUSED_RESULT bool NavigateToURLInSameBrowsingInstance(Shell* window,
                                                            const GURL& url);

// Helper function to checks for a subframe navigation starting  in
// `start_site_instance` and results in an error page correctly transitions to
// `end_site_instance` based on whether error page isolation is enabled or not.
WARN_UNUSED_RESULT bool IsExpectedSubframeErrorTransition(
    SiteInstance* start_site_instance,
    SiteInstance* end_site_instance);

// Open a new popup passing no URL to window.open, which results in a blank page
// and no last committed entry. Returns the newly created shell. Also saves the
// reference to the opened window in the "last_opened_window" variable in JS.
Shell* OpenBlankWindow(WebContentsImpl* web_contents);

// Creates compact textual representations of the state of the frame tree that
// is appropriate for use in assertions.
//
// The diagrams show frame tree structure, the SiteInstance of current frames,
// presence of pending frames, and the SiteInstances of any and all proxies.
// They look like this:
//
//        Site A (D pending) -- proxies for B C
//          |--Site B --------- proxies for A C
//          +--Site C --------- proxies for B A
//               |--Site A ---- proxies for B
//               +--Site A ---- proxies for B
//                    +--Site A -- proxies for B
//       Where A = http://127.0.0.1/
//             B = http://foo.com/ (no process)
//             C = http://bar.com/
//             D = http://next.com/
//
// SiteInstances are assigned single-letter names (A, B, C) which are remembered
// across invocations of the pretty-printer.
class FrameTreeVisualizer {
 public:
  FrameTreeVisualizer();
  ~FrameTreeVisualizer();

  // Formats and returns a diagram for the provided FrameTreeNode.
  std::string DepictFrameTree(FrameTreeNode* root);

 private:
  // Assign or retrive the abbreviated short name (A, B, C) for a site instance.
  std::string GetName(SiteInstance* site_instance);

  // Elements are site instance ids. The index of the SiteInstance in the vector
  // determines the abbreviated name (0->A, 1->B) for that SiteInstance.
  std::vector<int> seen_site_instance_ids_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeVisualizer);
};

// Uses FrameTreeVisualizer to draw a text representation of the FrameTree that
// is appropriate for use in assertions. If you are going to depict multiple
// trees in a single test, you might want to construct a longer-lived instance
// of FrameTreeVisualizer as this will ensure consistent naming of the site
// instances across all calls.
std::string DepictFrameTree(FrameTreeNode& root);

// Uses window.open to open a popup from the frame |opener| with the specified
// |url|, |name| and window |features|. |expect_return_from_window_open| is used
// to indicate if the caller expects window.open() to return a non-null value.
// Waits for the navigation to |url| to finish and then returns the new popup's
// Shell.  Note that since this navigation to |url| is renderer-initiated, it
// won't cause a process swap unless used in --site-per-process mode.
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name,
                 const std::string& features,
                 bool expect_return_from_window_open);

// Same as above, but with an empty |features| and
// |expect_return_from_window_open| assumed to be true..
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name);

// Helper for mocking choosing a file via a file dialog.
class FileChooserDelegate : public WebContentsDelegate {
 public:
  // Constructs a WebContentsDelegate that mocks a file dialog.
  // The mocked file dialog will always reply that the user selected |file|.
  // |callback| is invoked when RunFileChooser() is called.
  FileChooserDelegate(const base::FilePath& file, base::OnceClosure callback);
  ~FileChooserDelegate() override;

  // Implementation of WebContentsDelegate::RunFileChooser.
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

  // The params passed to RunFileChooser.
  const blink::mojom::FileChooserParams& params() const { return *params_; }

 private:
  base::FilePath file_;
  base::OnceClosure callback_;
  blink::mojom::FileChooserParamsPtr params_;
};

// This class is a TestNavigationManager that only monitors notifications within
// the given frame tree node.
class FrameTestNavigationManager : public TestNavigationManager {
 public:
  FrameTestNavigationManager(int frame_tree_node_id,
                             WebContents* web_contents,
                             const GURL& url);

 private:
  // TestNavigationManager:
  bool ShouldMonitorNavigation(NavigationHandle* handle) override;

  // Notifications are filtered so only this frame is monitored.
  int filtering_frame_tree_node_id_;

  DISALLOW_COPY_AND_ASSIGN(FrameTestNavigationManager);
};

// An observer that can wait for a specific URL to be committed in a specific
// frame.
// Note: it does not track the start of a navigation, unlike other observers.
class UrlCommitObserver : WebContentsObserver {
 public:
  explicit UrlCommitObserver(FrameTreeNode* frame_tree_node, const GURL& url);
  ~UrlCommitObserver() override;

  void Wait();

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  // The id of the FrameTreeNode in which navigations are peformed.
  int frame_tree_node_id_;

  // The URL this observer is expecting to be committed.
  GURL url_;

  // The RunLoop used to spin the message loop.
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(UrlCommitObserver);
};

// Waits for a kill of the given RenderProcessHost and returns the
// BadMessageReason that caused a //content-triggerred kill.
//
// Example usage:
//   RenderProcessHostBadIpcMessageWaiter kill_waiter(render_process_host);
//   ... test code that triggers a renderer kill ...
//   EXPECT_EQ(bad_message::RFH_INVALID_ORIGIN_ON_COMMIT, kill_waiter.Wait());
//
// Tests that don't expect kills (e.g. tests where a renderer process exits
// normally, like RenderFrameHostManagerTest.ProcessExitWithSwappedOutViews)
// should use RenderProcessHostWatcher instead of
// RenderProcessHostBadIpcMessageWaiter.
class RenderProcessHostBadIpcMessageWaiter {
 public:
  explicit RenderProcessHostBadIpcMessageWaiter(
      RenderProcessHost* render_process_host);

  // Waits until the renderer process exits.  Returns the bad message that made
  // //content kill the renderer.  |base::nullopt| is returned if the renderer
  // was killed outside of //content or exited normally.
  base::Optional<bad_message::BadMessageReason> Wait() WARN_UNUSED_RESULT;

 private:
  RenderProcessHostKillWaiter internal_waiter_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostBadIpcMessageWaiter);
};

class ShowPopupWidgetWaiter
    : public WebContentsObserver,
      public blink::mojom::PopupWidgetHostInterceptorForTesting {
 public:
  ShowPopupWidgetWaiter(WebContents* web_contents,
                        RenderFrameHostImpl* frame_host);
  ~ShowPopupWidgetWaiter() override;

  gfx::Rect last_initial_rect() const { return initial_rect_; }

  int last_routing_id() const { return routing_id_; }

  // Waits until a popup request is received.
  void Wait();

  // Stops observing new messages.
  void Stop();

 private:

  // WebContentsObserver:
#if defined(OS_MAC) || defined(OS_ANDROID)
  bool ShowPopupMenu(
      RenderFrameHost* render_frame_host,
      mojo::PendingRemote<blink::mojom::PopupMenuClient>* popup_client,
      const gfx::Rect& bounds,
      int32_t item_height,
      double font_size,
      int32_t selected_item,
      std::vector<blink::mojom::MenuItemPtr>* menu_items,
      bool right_aligned,
      bool allow_multiple_selection) override;
#endif

  // Callback bound for creating a popup widget.
  void DidCreatePopupWidget(RenderWidgetHostImpl* render_widget_host);

  // blink::mojom::PopupWidgetHostInterceptorForTesting:
  blink::mojom::PopupWidgetHost* GetForwardingInterface() override;
  void ShowPopup(const gfx::Rect& initial_rect,
                 ShowPopupCallback callback) override;

  base::RunLoop run_loop_;
  gfx::Rect initial_rect_;
  int32_t routing_id_ = MSG_ROUTING_NONE;
  int32_t process_id_ = 0;
  RenderFrameHostImpl* frame_host_;

  DISALLOW_COPY_AND_ASSIGN(ShowPopupWidgetWaiter);
};

// A BrowserMessageFilter that drops a blacklisted message.
class DropMessageFilter : public BrowserMessageFilter {
 public:
  DropMessageFilter(uint32_t message_class, uint32_t drop_message_id);

 protected:
  ~DropMessageFilter() override;

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;

  const uint32_t drop_message_id_;

  DISALLOW_COPY_AND_ASSIGN(DropMessageFilter);
};

// A BrowserMessageFilter that observes a message without handling it, and
// reports when it was seen.
class ObserveMessageFilter : public BrowserMessageFilter {
 public:
  ObserveMessageFilter(uint32_t message_class, uint32_t watch_message_id);

  bool has_received_message() { return received_; }

  // Spins a RunLoop until the message is observed.
  void Wait();

 protected:
  ~ObserveMessageFilter() override;

  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  void QuitWait();

  const uint32_t watch_message_id_;
  bool received_ = false;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ObserveMessageFilter);
};

// This observer waits until WebContentsObserver::OnRendererUnresponsive
// notification.
class UnresponsiveRendererObserver : public WebContentsObserver {
 public:
  explicit UnresponsiveRendererObserver(WebContents* web_contents);
  ~UnresponsiveRendererObserver() override;

  RenderProcessHost* Wait(base::TimeDelta timeout = base::TimeDelta::Max());

 private:
  // WebContentsObserver:
  void OnRendererUnresponsive(RenderProcessHost* render_process_host) override;

  RenderProcessHost* captured_render_process_host_ = nullptr;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(UnresponsiveRendererObserver);
};

// Helper class that overrides the JavaScriptDialogManager of a WebContents
// to endlessly block on beforeunload.
class BeforeUnloadBlockingDelegate : public JavaScriptDialogManager,
                                     public WebContentsDelegate {
 public:
  explicit BeforeUnloadBlockingDelegate(WebContentsImpl* web_contents);
  ~BeforeUnloadBlockingDelegate() override;
  void Wait();

  // WebContentsDelegate

  JavaScriptDialogManager* GetJavaScriptDialogManager(
      WebContents* source) override;

  // JavaScriptDialogManager

  void RunJavaScriptDialog(WebContents* web_contents,
                           RenderFrameHost* render_frame_host,
                           JavaScriptDialogType dialog_type,
                           const base::string16& message_text,
                           const base::string16& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;

  void RunBeforeUnloadDialog(WebContents* web_contents,
                             RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;

  bool HandleJavaScriptDialog(WebContents* web_contents,
                              bool accept,
                              const base::string16* prompt_override) override;

  void CancelDialogs(WebContents* web_contents, bool reset_state) override {}

 private:
  WebContentsImpl* web_contents_;

  DialogClosedCallback callback_;

  std::unique_ptr<base::RunLoop> run_loop_ = std::make_unique<base::RunLoop>();

  DISALLOW_COPY_AND_ASSIGN(BeforeUnloadBlockingDelegate);
};

// A helper class to get DevTools inspector log messages (e.g. network errors).
class DevToolsInspectorLogWatcher : public DevToolsAgentHostClient {
 public:
  explicit DevToolsInspectorLogWatcher(WebContents* web_contents);
  ~DevToolsInspectorLogWatcher() override;

  void FlushAndStopWatching();
  std::string last_message() { return last_message_; }

  // DevToolsAgentHostClient:
  void DispatchProtocolMessage(DevToolsAgentHost* host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(DevToolsAgentHost* host) override;

 private:
  scoped_refptr<DevToolsAgentHost> host_;
  base::RunLoop run_loop_enable_log_;
  base::RunLoop run_loop_disable_log_;
  std::string last_message_;
};

// Captures various properties of the NavigationHandle on DidFinishNavigation.
// By default, captures the next navigation and waits until the navigation
// completely loads. Can be configured to not wait for load to finish, and also
// to capture properties for multiple navigations, as we save the values in
// arrays.
class FrameNavigateParamsCapturer : public WebContentsObserver {
 public:
  // Observes navigation for the specified |node|.
  explicit FrameNavigateParamsCapturer(FrameTreeNode* node);
  ~FrameNavigateParamsCapturer() override;

  // Start waiting for |navigations_remaining_| navigations to finish (and for
  // load to finish if |wait_for_load_| is true).
  void Wait();

  // Sets the number of navigations to wait for.
  void set_navigations_remaining(int count) {
    DCHECK_GE(count, 0);
    navigations_remaining_ = count;
  }

  // Sets |wait_for_load_| to determine whether to stop waiting when we receive
  // DidFInishNavigation or DidStopLoading.
  void set_wait_for_load(bool wait_for_load) { wait_for_load_ = wait_for_load; }

  // Gets various captured parameters from the last navigation.
  // Must only be called when we only capture a single navigation.
  ui::PageTransition transition() const {
    EXPECT_EQ(1U, transitions_.size());
    return transitions_[0];
  }
  NavigationType navigation_type() const {
    EXPECT_EQ(1U, navigation_types_.size());
    return navigation_types_[0];
  }
  bool is_same_document() const {
    EXPECT_EQ(1U, is_same_documents_.size());
    return is_same_documents_[0];
  }
  bool is_renderer_initiated() const {
    EXPECT_EQ(1U, is_renderer_initiateds_.size());
    return is_renderer_initiateds_[0];
  }
  bool did_replace_entry() const {
    EXPECT_EQ(1U, did_replace_entries_.size());
    return did_replace_entries_[0];
  }
  bool has_user_gesture() const {
    EXPECT_EQ(1U, has_user_gestures_.size());
    return has_user_gestures_[0];
  }

  // Gets various captured parameters from all observed navigations.
  const std::vector<ui::PageTransition>& transitions() { return transitions_; }
  const std::vector<GURL>& urls() { return urls_; }
  const std::vector<NavigationType>& navigation_types() {
    return navigation_types_;
  }
  const std::vector<bool>& is_same_documents() { return is_same_documents_; }
  const std::vector<bool>& did_replace_entries() {
    return did_replace_entries_;
  }
  const std::vector<bool>& has_user_gestures() { return has_user_gestures_; }

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void DidStopLoading() override;

  // The id of the FrameTreeNode whose navigations to observe.
  int frame_tree_node_id_;

  // How many navigations remain to capture.
  int navigations_remaining_ = 1;

  // Whether to also wait for the load to complete.
  bool wait_for_load_ = true;

  // The saved properties of the NavigationHandle, captured on
  // DidFinishNavigation. When Wait() finishes, these arrays should contain
  // |navigations_remaining_|, as we always capture them for each navigations.
  std::vector<ui::PageTransition> transitions_;
  std::vector<GURL> urls_;
  std::vector<NavigationType> navigation_types_;
  std::vector<bool> is_same_documents_;
  std::vector<bool> did_replace_entries_;
  std::vector<bool> is_renderer_initiateds_;
  std::vector<bool> has_user_gestures_;

  base::RunLoop loop_;
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
