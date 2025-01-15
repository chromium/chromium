// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_UTILS_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/common/page_type.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class FilePath;

namespace apple {
class ScopedObjCClassSwizzler;
}  // namespace apple
}  // namespace base

namespace gfx {
class Point;
#if BUILDFLAG(IS_MAC)
class Range;
#endif
class Rect;
}  // namespace gfx

namespace net {
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server
}  // namespace net

// A collections of functions designed for use with content_shell based browser
// tests.
// Note: if a function here also works with browser_tests, it should be in
// content\public\test\browser_test_utils.h

namespace content {
class RenderFrameHost;
class RenderWidgetHost;
class Shell;
class ToRenderFrameHost;
class WebContents;

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// content/test/data/dir/<file>
// The returned path is FilePath format.
//
// A null |dir| indicates the root directory - i.e.
// content/test/data/<file>
base::FilePath GetTestFilePath(const char* dir, const char* file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
//
// A null |dir| indicates the root directory - i.e.
// content/test/data/<file>
GURL GetTestUrl(const char* dir, const char* file);

// Navigates |window| to |url|, blocking until the navigation finishes. Returns
// true if the page was loaded successfully and the last committed URL matches
// |url|.  This is a browser-initiated navigation that simulates a user typing
// |url| into the address bar.
//
// Tests should ensure that NavigateToURL succeeds.  If the URL that will
// eventually commit is different from |url|, such as with redirects, use the
// version below which also takes the expected commit URL.  If the navigation
// will not result in a commit, such as a download or a 204 response, use
// NavigateToURLAndExpectNoCommit() instead.
[[nodiscard]] bool NavigateToURL(Shell* window, const GURL& url);

// Same as above, but takes in an additional URL, |expected_commit_url|, to
// which the navigation should eventually commit.  This is useful for cases
// like redirects, where navigation starts on one URL but ends up committing a
// different URL.  This function will return true if navigating to |url|
// results in a successful commit to |expected_commit_url|.
[[nodiscard]] bool NavigateToURL(Shell* window,
                                 const GURL& url,
                                 const GURL& expected_commit_url);

// Navigates |window| to |url|, blocking until the given number of navigations
// finishes. If |ignore_uncommitted_navigations| is true, then an aborted
// navigation also counts toward |number_of_navigations| being complete.
void NavigateToURLBlockUntilNavigationsComplete(
    Shell* window,
    const GURL& url,
    int number_of_navigations,
    bool ignore_uncommitted_navigations = true);

// Navigates |window| to |url|, blocks until the navigation finishes, and
// checks that the navigation did not commit (e.g., due to a crash or
// download).
[[nodiscard]] bool NavigateToURLAndExpectNoCommit(Shell* window,
                                                  const GURL& url);

// Reloads |window|, blocking until the given number of navigations finishes.
void ReloadBlockUntilNavigationsComplete(Shell* window,
                                         int number_of_navigations);

// Reloads |window| with bypassing cache flag, and blocks until the given number
// of navigations finishes.
void ReloadBypassingCacheBlockUntilNavigationsComplete(
    Shell* window,
    int number_of_navigations);

// A class to help with waiting for at least one javascript dialog to be
// requested.
//
// On creation or Restart, it uses set_dialog_request_callback to
// capture any future dialog request. Calling Wait() will
// either return immediately because a dialog has already been called or it will
// wait, processing events until one is requested.
//
// That means, object should be constructed, or Restart() called, before section
// that could request a modal dialog.
class AppModalDialogWaiter {
 public:
  explicit AppModalDialogWaiter(Shell* shell);
  void Restart();
  void Wait();

  bool WasDialogRequestedCallbackCalled() {
    return was_dialog_request_callback_called_;
  }

 private:
  void EarlyCallback();
  bool was_dialog_request_callback_called_ = false;
  raw_ptr<Shell> shell_;
};

// Extends the ToRenderFrameHost mechanism to content::Shells.
RenderFrameHost* ConvertToRenderFrameHost(Shell* shell);

// Writes an entry with the name and id of the first camera to the logs or
// an entry indicating that no camera is available. This must be invoked from
// the test method body, because at the time of invocation of
// testing::Test::SetUp() the BrowserMainLoop does not yet exist.
void LookupAndLogNameAndIdOfFirstCamera();

// Used to wait for a new Shell window to be created. Instantiate this object
// before the operation that will create the window.
class ShellAddedObserver {
 public:
  ShellAddedObserver();

  ShellAddedObserver(const ShellAddedObserver&) = delete;
  ShellAddedObserver& operator=(const ShellAddedObserver&) = delete;

  ~ShellAddedObserver();

  // Will run a message loop to wait for the new window if it hasn't been
  // created since the constructor.
  Shell* GetShell();

 private:
  void ShellCreated(Shell* shell);

  raw_ptr<Shell, AcrossTasksDanglingUntriaged> shell_ = nullptr;
  std::unique_ptr<base::RunLoop> runner_;
};

#if BUILDFLAG(IS_MAC)
// An observer of the RenderWidgetHostViewCocoa which is the NSView
// corresponding to the page.
class RenderWidgetHostViewCocoaObserver {
 public:
  // The method name for 'didAddSubview'.
  static constexpr char kDidAddSubview[] = "didAddSubview:";
  static constexpr char kShowDefinitionForAttributedString[] =
      "showDefinitionForAttributedString:atPoint:";

  // Returns the method swizzler for the given |method_name|. This is useful
  // when the original implementation of the method is needed.
  static base::apple::ScopedObjCClassSwizzler* GetSwizzler(
      const std::string& method_name);

  // Returns the unique RenderWidgetHostViewCocoaObserver instance (if any) for
  // the given WebContents. There can be at most one observer per WebContents
  // and to create a new observer the older one has to be deleted first.
  static RenderWidgetHostViewCocoaObserver* GetObserver(
      WebContents* web_contents);

  explicit RenderWidgetHostViewCocoaObserver(WebContents* web_contents);

  RenderWidgetHostViewCocoaObserver(const RenderWidgetHostViewCocoaObserver&) =
      delete;
  RenderWidgetHostViewCocoaObserver& operator=(
      const RenderWidgetHostViewCocoaObserver&) = delete;

  virtual ~RenderWidgetHostViewCocoaObserver();

  // Called when a new NSView is added as a subview of RWHVCocoa.
  // |rect_in_root_view| represents the bounds of the NSView in RWHVCocoa
  // coordinates. The view will be dismissed shortly after this call.
  virtual void DidAddSubviewWillBeDismissed(
      const gfx::Rect& rect_in_root_view) {}
  // Called when RenderWidgeHostViewCocoa is asked to show definition of
  // |for_word| using Mac's dictionary popup.
  virtual void OnShowDefinitionForAttributedString(
      const std::string& for_word) {}

  WebContents* web_contents() const { return web_contents_; }

 private:
  static void SetUpSwizzlers();

  static std::map<std::string,
                  std::unique_ptr<base::apple::ScopedObjCClassSwizzler>>
      rwhvcocoa_swizzlers_;
  static std::map<WebContents*, RenderWidgetHostViewCocoaObserver*> observers_;

  const raw_ptr<WebContents> web_contents_;
};

void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds);

// This method will request the string (word) at |point| inside the |rwh| where
// |point| is with respect to the |rwh| coordinates. |result_callback| is called
// with the word as well as |baselinePoint| when the result comes back from the
// renderer. The baseline point is the position of the pop-up in AppKit
// coordinate system (inverted y-axis).
void GetStringAtPointForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Point& point,
    base::OnceCallback<void(const std::string&, const gfx::Point&)>
        result_callback);

// This method will request the string identified by |range| inside the |rwh|.
// When the result comes back, |result_callback| is invoked with the given text
// and its position in AppKit coordinates (inverted-y axis).
void GetStringFromRangeForRenderWidget(
    RenderWidgetHost* rwh,
    const gfx::Range& range,
    base::OnceCallback<void(const std::string&, const gfx::Point&)>
        result_callback);

#endif

// Adds http://<hostname_to_isolate>/ to the list of origins that require
// isolation (for each of the hostnames in the |hostnames_to_isolate| vector).
//
// To ensure that the isolation applies to subsequent navigations in
// |web_contents|, this function forces a BrowsingInstance swap by performing
// one or two browser-initiated navigations in |web_contents| to another,
// random, guid-based hostname.
void IsolateOriginsForTesting(
    net::test_server::EmbeddedTestServer* embedded_test_server,
    WebContents* web_contents,
    std::vector<std::string> hostnames_to_isolate);

// Same as above, but takes full origins as input.  In particular, this version
// doesn't assume HTTP, so it can be used for also isolating HTTPS origins.
void IsolateOriginsForTesting(
    net::test_server::EmbeddedTestServer* embedded_test_server,
    WebContents* web_contents,
    std::vector<url::Origin> origins_to_isolate);

#if BUILDFLAG(IS_WIN)

void SetMockCursorPositionForTesting(WebContents* web_contents,
                                     const gfx::Point& position);

#endif  // BUILDFLAG(IS_WIN)

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_UTILS_H_
