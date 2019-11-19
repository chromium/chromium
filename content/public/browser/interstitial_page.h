// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INTERSTITIAL_PAGE_H_
#define CONTENT_PUBLIC_BROWSER_INTERSTITIAL_PAGE_H_

#include "content/common/content_export.h"

class GURL;

namespace gfx {
class Size;
}

namespace content {

class InterstitialPageDelegate;
class RenderFrameHost;
class WebContents;

// This class is used for showing interstitial pages, pages that show some
// informative message asking for user validation before reaching the target
// page. (Navigating to a page served over bad HTTPS or a page containing
// malware are typical cases where an interstitial is required.)
//
// If specified in the Create function, this class creates a navigation entry so
// that when the interstitial shows, the current entry is the target URL.
//
// InterstitialPage instances take care of deleting themselves when closed
// through a navigation, the WebContents closing them or the tab containing them
// being closed.

class InterstitialPage {
 public:
  // Creates an interstitial page to show in |web_contents|. |new_navigation|
  // should be set to true when the interstitial is caused by loading a new
  // page, in which case a temporary navigation entry is created with the URL
  // |url| and added to the navigation controller (so the interstitial page
  // appears as a new navigation entry). |new_navigation| should be false when
  // the interstitial was triggered by a loading a sub-resource in a page. Takes
  // ownership of |delegate|.
  //
  // Reloading the interstitial page will result in a new navigation to |url|.
  CONTENT_EXPORT static InterstitialPage* Create(
      WebContents* web_contents,
      bool new_navigation,
      const GURL& url,
      InterstitialPageDelegate* delegate);

  // Returns the InterstitialPage, if any, associated with the specified
  // |web_contents|. Note: This returns a value from the time the interstitial
  // page has Show() called on it.
  //
  // Compare to WebContents::GetInterstitialPage.
  CONTENT_EXPORT static InterstitialPage* GetInterstitialPage(
      WebContents* web_contents);

  // Returns the InterstitialPage that hosts the RenderFrameHost, or nullptr if
  // |rfh| is not a part of any InterstitialPage.
  CONTENT_EXPORT static InterstitialPage* FromRenderFrameHost(
      RenderFrameHost* rfh);

  virtual ~InterstitialPage() {}

  // Shows the interstitial page in the tab.
  virtual void Show() = 0;

  // Hides the interstitial page.
  virtual void Hide() = 0;

  // Reverts to the page showing before the interstitial.
  // Delegates should call this method when the user has chosen NOT to proceed
  // to the target URL.
  // Warning: if |new_navigation| was set to true in the constructor, 'this'
  //          will be deleted when this method returns.
  virtual void DontProceed() = 0;

  // Delegates should call this method when the user has chosen to proceed to
  // the target URL.
  // Warning: 'this' has been deleted when this method returns.
  virtual void Proceed() = 0;

  // Sizes the RenderViewHost showing the actual interstitial page contents.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Sets the focus to the interstitial.
  virtual void Focus() = 0;

  // Get the WebContents in which this interstitial is shown. Warning: Frames
  // in the intersitital are NOT visible through WebContentObservers' normal
  // notifications (e.g. RenderFrameDeleted). The only sensible use of this
  // returned WebContents is to add a WebContentObserver and listen for the
  // DidAttachInterstitialPage or DidDetachInterstitialPage notifications.
  virtual WebContents* GetWebContents() = 0;

  // Gets the RenderFrameHost associated with the interstitial page's main
  // frame. May return nullptr if the interstitial is already hidden.
  virtual RenderFrameHost* GetMainFrame() = 0;

  virtual InterstitialPageDelegate* GetDelegateForTesting() = 0;
  virtual void DontCreateViewForTesting() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INTERSTITIAL_PAGE_H_
