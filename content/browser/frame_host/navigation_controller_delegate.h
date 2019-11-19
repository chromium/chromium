// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_

#include <stdint.h>

#include <string>
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/common/previews_state.h"

namespace content {

struct LoadCommittedDetails;
class FrameTree;
class InterstitialPage;
class InterstitialPageImpl;
class RenderFrameHost;
class RenderViewHost;
class WebContents;

// Interface for objects embedding a NavigationController to provide the
// functionality NavigationController needs.
// TODO(nasko): This interface should exist for short amount of time, while
// we transition navigation code from WebContents to Navigator.
class NavigationControllerDelegate {
 public:
  virtual ~NavigationControllerDelegate() {}

  // Duplicates of WebContents methods.
  virtual RenderViewHost* GetRenderViewHost() = 0;
  virtual InterstitialPage* GetInterstitialPage() = 0;
  virtual const std::string& GetContentsMimeType() = 0;
  virtual void NotifyNavigationStateChanged(InvalidateTypes changed_flags) = 0;
  virtual void Stop() = 0;
  virtual bool IsBeingDestroyed() = 0;
  virtual bool CanOverscrollContent() const = 0;

  // Methods from WebContentsImpl that NavigationControllerImpl needs to
  // call.
  virtual FrameTree* GetFrameTree() = 0;
  virtual void NotifyBeforeFormRepostWarningShow() = 0;
  virtual void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) = 0;
  virtual void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) = 0;
  virtual void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) = 0;
  virtual void NotifyNavigationEntriesDeleted() = 0;
  virtual void SetHistoryOffsetAndLength(int history_offset,
                                         int history_length) = 0;
  virtual void ActivateAndShowRepostFormWarningDialog() = 0;
  virtual bool HasAccessedInitialDocument() = 0;

  // Returns whether URLs for aborted browser-initiated navigations should be
  // preserved in the omnibox.  Defaults to false.
  virtual bool ShouldPreserveAbortedURLs() = 0;

  // TODO(crbug.com/934637): Remove when pdf and any inner web contents user
  // gesture is properly propagated.
  virtual bool HadInnerWebContents() = 0;

  // This method is needed, since we are no longer guaranteed that the
  // embedder for NavigationController will be a WebContents object.
  virtual WebContents* GetWebContents() = 0;

  // Methods needed by InterstitialPageImpl.
  virtual bool IsHidden() = 0;
  virtual void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) = 0;
  virtual void AttachInterstitialPage(
      InterstitialPageImpl* interstitial_page) = 0;
  virtual void DidProceedOnInterstitial() = 0;
  virtual void DetachInterstitialPage(bool has_focus) = 0;

  virtual void UpdateOverridingUserAgent() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
