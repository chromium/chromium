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
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "url/gurl.h"

namespace content {

class FrameTreeNode;
class RenderFrameHost;
class Shell;
class SiteInstance;
class ToRenderFrameHost;

// Navigates the frame represented by |node| to |url|, blocking until the
// navigation finishes.
void NavigateFrameToURL(FrameTreeNode* node, const GURL& url);

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

// Uses window.open to open a popup from the frame |opener| with the specified
// |url| and |name|.   Waits for the navigation to |url| to finish and then
// returns the new popup's Shell.  Note that since this navigation to |url| is
// renderer-initiated, it won't cause a process swap unless used in
// --site-per-process mode.
Shell* OpenPopup(const ToRenderFrameHost& opener,
                 const GURL& url,
                 const std::string& name);

// Helper for mocking choosing a file via a file dialog.
class FileChooserDelegate : public WebContentsDelegate {
 public:
  // Constructs a WebContentsDelegate that mocks a file dialog.
  // The mocked file dialog will always reply that the user selected |file|.
  explicit FileChooserDelegate(const base::FilePath& file);
  ~FileChooserDelegate() override;

  // Implementation of WebContentsDelegate::RunFileChooser.
  void RunFileChooser(RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;

  // Whether the file dialog was shown.
  bool file_chosen() const { return file_chosen_; }

  // The params passed to RunFileChooser.
  const blink::mojom::FileChooserParams& params() const { return *params_; }

 private:
  base::FilePath file_;
  bool file_chosen_;
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
//   RenderProcessHostKillWaiter kill_waiter(render_process_host);
//   ... test code that triggers a renderer kill ...
//   EXPECT_EQ(bad_message::RFH_INVALID_ORIGIN_ON_COMMIT, kill_waiter.Wait());
//
// Tests that don't expect kills (e.g. tests where a renderer process exits
// normally, like RenderFrameHostManagerTest.ProcessExitWithSwappedOutViews)
// should use RenderProcessHostWatcher instead of RenderProcessHostKillWaiter.
class RenderProcessHostKillWaiter {
 public:
  explicit RenderProcessHostKillWaiter(RenderProcessHost* render_process_host);

  // Waits until the renderer process exits.  Returns the bad message that made
  // //content kill the renderer.  |base::nullopt| is returned if the renderer
  // was killed outside of //content or exited normally.
  base::Optional<bad_message::BadMessageReason> Wait() WARN_UNUSED_RESULT;

 private:
  RenderProcessHostWatcher exit_watcher_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostKillWaiter);
};

class ShowWidgetMessageFilter : public content::BrowserMessageFilter {
 public:
  ShowWidgetMessageFilter();

  bool OnMessageReceived(const IPC::Message& message) override;

  gfx::Rect last_initial_rect() const { return initial_rect_; }

  int last_routing_id() const { return routing_id_; }

  void Wait();

  void Reset();

 private:
  ~ShowWidgetMessageFilter() override;

  void OnShowWidget(int route_id, const gfx::Rect& initial_rect);

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params);
#endif

  void OnShowWidgetOnUI(int route_id, const gfx::Rect& initial_rect);

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  gfx::Rect initial_rect_;
  int routing_id_ = MSG_ROUTING_NONE;

  DISALLOW_COPY_AND_ASSIGN(ShowWidgetMessageFilter);
};

// A BrowserMessageFilter that drops SwapOut ACK messages.
class SwapoutACKMessageFilter : public BrowserMessageFilter {
 public:
  SwapoutACKMessageFilter();

 protected:
  ~SwapoutACKMessageFilter() override;

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override;
  DISALLOW_COPY_AND_ASSIGN(SwapoutACKMessageFilter);
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_INTERNAL_H_
