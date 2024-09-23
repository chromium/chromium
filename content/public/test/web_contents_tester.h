// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_
#define CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "ui/base/page_transition_types.h"

class GURL;
class SkBitmap;

namespace gfx {
class Size;
}

namespace content {

class BrowserContext;
class NavigationSimulator;

// This interface allows embedders of content/ to write tests that depend on a
// test version of WebContents.  This interface can be retrieved from any
// WebContents that was retrieved via a call to
// RenderViewHostTestHarness::GetWebContents() (directly or indirectly) or
// constructed explicitly via CreateTestWebContents.
//
// Tests within content/ can directly static_cast WebContents objects retrieved
// or created as described above to TestWebContents.
//
// Design note: We considered two alternatives to this separate test interface
// approach:
//
// a) Define a TestWebContents interface that inherits from WebContents, and
// have the concrete TestWebContents inherit from it as well as from
// WebContentsImpl.  This approach was discarded as it introduces a diamond
// inheritance pattern, which means we wouldn't be e.g. able to downcast from
// WebContents to WebContentsImpl using static_cast.
//
// b) Define a TestWebContents interface that inherits from WebContents, and
// have the concrete TestWebContents implement it, using composition of a
// WebContentsImpl to implement most methods.  This approach was discarded as
// there is a fundamental assumption in content/ that a WebContents* can be
// downcast to a WebContentsImpl*, and this wouldn't be true for TestWebContents
// objects.
//
// Tests that use a TestWebContents must also use TestRenderViewHost and
// TestRenderFrameHost. They can do so by instantiating a
// RenderViewHostTestEnabler.
class WebContentsTester {
 public:
  // Retrieves a WebContentsTester to drive tests of the specified WebContents.
  // As noted above you need to be sure the 'contents' object supports testing,
  // i.e. is either created using one of the Create... functions below, or is
  // retrieved via RenderViewHostTestHarness::GetWebContents().
  static WebContentsTester* For(WebContents* contents);

  // Creates a WebContents enabled for testing.
  static std::unique_ptr<WebContents> CreateTestWebContents(
      BrowserContext* browser_context,
      scoped_refptr<SiteInstance> instance);

  // Creates a WebContents enabled for testing with the given params.
  static WebContents* CreateTestWebContents(
      const WebContents::CreateParams& params);

  // Simulates the appropriate `blink::WebView` (pending if any, current
  // otherwise) sending a navigate notification for the NavigationController
  // pending entry.
  virtual void CommitPendingNavigation() = 0;

  // Creates a pending navigation to the given URL with the default parameters
  // and then commits the load with a page ID one larger than any seen. This
  // emulates what happens on a new navigation.
  // Default parameter transition allows the transition type to be controlled
  // if needed.
  virtual void NavigateAndCommit(
      const GURL& url,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK) = 0;

  // Creates a pending navigation to the given URL with the default parameters
  // and then aborts it with the given |error_code|.
  virtual void NavigateAndFail(const GURL& url, int error_code) = 0;

  // Sets the loading state to the given value.
  virtual void TestSetIsLoading(bool value) = 0;

  // Simulate this WebContents' main frame having an opener that points to the
  // main frame of |opener|.
  virtual void SetOpener(WebContents* opener) = 0;

  // Sets the process state for the primary main frame renderer.
  virtual void SetIsCrashed(base::TerminationStatus status, int error_code) = 0;

  // Returns headers that were passed in the previous SaveFrameWithHeaders(...)
  // call.
  virtual const std::string& GetSaveFrameHeaders() = 0;

  // Returns the suggested file name passed in the SaveFrameWithHeaders call.
  virtual const std::u16string& GetSuggestedFileName() = 0;

  // Returns whether a download request triggered via DownloadImage() is in
  // progress for |url|.
  virtual bool HasPendingDownloadImage(const GURL& url) = 0;

  // Simulates a request completion for DownloadImage(). For convenience, it
  // returns whether an actual download associated to |url| was pending.
  virtual bool TestDidDownloadImage(
      const GURL& url,
      int http_status_code,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes) = 0;

  // Simulates initial favicon urls set.
  virtual void TestSetFaviconURL(
      const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) = 0;

  // Simulates favicon urls update.
  virtual void TestUpdateFaviconURL(
      const std::vector<blink::mojom::FaviconURLPtr>& favicon_urls) = 0;

  // Sets the return value of GetLastCommittedUrl() of TestWebContents.
  virtual void SetLastCommittedURL(const GURL& url) = 0;

  // Sets the return value of GetTitle() of TestWebContents. Once set, the real
  // title will never be returned.
  virtual void SetTitle(const std::u16string& new_title) = 0;

  // Sets the return value of GetContentsMimeType().
  virtual void SetMainFrameMimeType(const std::string& mime_type) = 0;

  // Sets the main frame size.
  virtual void SetMainFrameSize(const gfx::Size& frame_size) = 0;

  // Change currently audible state for testing. This will cause all relevant
  // notifications to fire as well.
  virtual void SetIsCurrentlyAudible(bool audible) = 0;

  // Simulates an input event from the user.
  virtual void TestDidReceiveMouseDownEvent() = 0;

  // Simulates successfully finishing a load.
  virtual void TestDidFinishLoad(const GURL& url) = 0;

  // Simulates terminating an load with a network error.
  virtual void TestDidFailLoadWithError(const GURL& url, int error_code) = 0;

  // Simulates the first non-empty paint.
  virtual void TestDidFirstVisuallyNonEmptyPaint() = 0;

  // Returns whether PauseSubresourceLoading was called on this web contents.
  virtual bool GetPauseSubresourceLoadingCalled() = 0;

  // Resets the state around PauseSubresourceLoadingCalled.
  virtual void ResetPauseSubresourceLoadingCalled() = 0;

  // Sets the last active time ticks.
  virtual void SetLastActiveTimeTicks(
      base::TimeTicks last_active_time_ticks) = 0;

  // Sets the last active time.
  virtual void SetLastActiveTime(base::Time last_active_time) = 0;

  // Increments/decrements the number of frames with connected USB devices.
  virtual void TestIncrementUsbActiveFrameCount() = 0;
  virtual void TestDecrementUsbActiveFrameCount() = 0;

  // Increments/decrements the number of frames with connected HID devices.
  virtual void TestIncrementHidActiveFrameCount() = 0;
  virtual void TestDecrementHidActiveFrameCount() = 0;

  // Increments/decrements the number of frames actively using serial ports.
  virtual void TestIncrementSerialActiveFrameCount() = 0;
  virtual void TestDecrementSerialActiveFrameCount() = 0;

  // Increments/decrements the number of connected Bluetooth devices.
  virtual void TestIncrementBluetoothConnectedDeviceCount() = 0;
  virtual void TestDecrementBluetoothConnectedDeviceCount() = 0;

  // Indicates if this WebContents has been frozen via a call to
  // SetPageFrozen().
  virtual bool IsPageFrozen() = 0;

  // Starts prerendering a page with |url|, and returns the root frame tree node
  // id of the page. The page has a pending navigation in the root frame tree
  // node when this method returns.
  virtual FrameTreeNodeId AddPrerender(const GURL& url) = 0;
  // Starts prerendering a page, simulates a navigation to |url| in the main
  // frame and returns the main frame of the page after the navigation is
  // complete.
  virtual RenderFrameHost* AddPrerenderAndCommitNavigation(const GURL& url) = 0;
  // Starts prerendering a page, simulates a navigation to |url| in the main
  // frame and returns the simulator after the navigation is started.
  virtual std::unique_ptr<NavigationSimulator> AddPrerenderAndStartNavigation(
      const GURL& url) = 0;
  // Activates a prerendered page.
  virtual void ActivatePrerenderedPage(const GURL& url) = 0;

  // Returns the time that was set with SetTabSwitchStartTime, or a null
  // TimeTicks if it was never called.
  virtual base::TimeTicks GetTabSwitchStartTime() = 0;

  // Sets the return value for GetPictureInPictureOptions().
  virtual void SetPictureInPictureOptions(
      std::optional<blink::mojom::PictureInPictureWindowOptions> options) = 0;

  virtual bool GetOverscrollNavigationEnabled() = 0;

  // Sets return value for GetMediaCaptureRawDeviceIdsOpened(), keyed by `type`.
  virtual void SetMediaCaptureRawDeviceIdsOpened(
      blink::mojom::MediaStreamType type,
      std::vector<std::string> ids) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_
