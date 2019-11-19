// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_
#define CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/base/page_transition_types.h"

class GURL;
class SkBitmap;

namespace gfx {
class Size;
}

namespace net {
class HttpResponseHeaders;
}

namespace content {

class BrowserContext;
class NavigationHandle;
class RenderFrameHost;

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

  // Simulates the appropriate RenderView (pending if any, current otherwise)
  // sending a navigate notification for the NavigationController pending entry.
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
  // and then aborts it with the given |error_code| and |response_headers|.
  virtual void NavigateAndFail(
      const GURL& url,
      int error_code,
      scoped_refptr<net::HttpResponseHeaders> response_headers) = 0;

  // Sets the loading state to the given value.
  virtual void TestSetIsLoading(bool value) = 0;

  // Simulates a navigation with the given information.
  //
  // Guidance for calling these:
  // - nav_entry_id should be 0 if simulating a renderer-initiated navigation;
  //   if simulating a browser-initiated one, pass the GetUniqueID() value of
  //   the NavigationController's PendingEntry.
  // - did_create_new_entry should be true if simulating a navigation that
  //   created a new navigation entry; false for history navigations, reloads,
  //   and other navigations that don't affect the history list.
  virtual void TestDidNavigate(RenderFrameHost* render_frame_host,
                               int nav_entry_id,
                               bool did_create_new_entry,
                               const GURL& url,
                               ui::PageTransition transition) = 0;

  // Sets HttpResponseData on |navigation_handle|.
  virtual void SetHttpResponseHeaders(
      NavigationHandle* navigation_handle,
      scoped_refptr<net::HttpResponseHeaders> response_headers) = 0;

  // Simulate this WebContents' main frame having an opener that points to the
  // main frame of |opener|.
  virtual void SetOpener(WebContents* opener) = 0;

  // Returns headers that were passed in the previous SaveFrameWithHeaders(...)
  // call.
  virtual const std::string& GetSaveFrameHeaders() = 0;

  // Returns the suggested file name passed in the SaveFrameWithHeaders call.
  virtual const base::string16& GetSuggestedFileName() = 0;

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

  // Sets the return value of GetLastCommittedUrl() of TestWebContents.
  virtual void SetLastCommittedURL(const GURL& url) = 0;

  // Sets the return value of GetTitle() of TestWebContents. Once set, the real
  // title will never be returned.
  virtual void SetTitle(const base::string16& new_title) = 0;

  // Sets the return value of GetContentsMimeType().
  virtual void SetMainFrameMimeType(const std::string& mime_type) = 0;

  // Change currently audible state for testing. This will cause all relevant
  // notifications to fire as well.
  virtual void SetIsCurrentlyAudible(bool audible) = 0;

  // Simulates an input event from the user.
  virtual void TestDidReceiveInputEvent(blink::WebInputEvent::Type type) = 0;

  // Simulates successfully finishing a load.
  virtual void TestDidFinishLoad(const GURL& url) = 0;

  // Simulates terminating an load with a network error.
  virtual void TestDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description) = 0;

  // Returns whether PauseSubresourceLoading was called on this web contents.
  virtual bool GetPauseSubresourceLoadingCalled() = 0;

  // Resets the state around PauseSubresourceLoadingCalled.
  virtual void ResetPauseSubresourceLoadingCalled() = 0;

  // Sets the return value of GetPageImportanceSignals().
  virtual void SetPageImportanceSignals(PageImportanceSignals signals) = 0;

  // Sets the last active time.
  virtual void SetLastActiveTime(base::TimeTicks last_active_time) = 0;

  // Setting this to true will make IsConnectedToBluetoothDevice() return true,
  // setting it to false will make the value use the logic from WebContentsImpl.
  virtual void SetIsConnectedToBluetoothDevice(
      bool is_connected_to_bluetooth_device) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_
