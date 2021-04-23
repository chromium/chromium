// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_

#include <stdint.h>

#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "third_party/blink/public/common/loader/previews_state.h"

namespace content {

struct LoadCommittedDetails;
class WebContents;

// Interface for objects embedding a NavigationController to provide the
// functionality NavigationController needs.
class NavigationControllerDelegate {
 public:
  virtual ~NavigationControllerDelegate() {}

  // Duplicates of WebContents methods.
  virtual void NotifyNavigationStateChanged(InvalidateTypes changed_flags) = 0;
  virtual void Stop() = 0;
  virtual bool IsBeingDestroyed() = 0;

  // Methods from WebContentsImpl that NavigationControllerImpl needs to
  // call.
  virtual void NotifyBeforeFormRepostWarningShow() = 0;
  virtual void NotifyNavigationEntryCommitted(
      const LoadCommittedDetails& load_details) = 0;
  virtual void NotifyNavigationEntryChanged(
      const EntryChangedDetails& change_details) = 0;
  virtual void NotifyNavigationListPruned(
      const PrunedDetails& pruned_details) = 0;
  virtual void NotifyNavigationEntriesDeleted() = 0;
  virtual void ActivateAndShowRepostFormWarningDialog() = 0;

  // Returns whether URLs for aborted browser-initiated navigations should be
  // preserved in the omnibox.  Defaults to false.
  virtual bool ShouldPreserveAbortedURLs() = 0;

  // This method is needed, since we are no longer guaranteed that the
  // embedder for NavigationController will be a WebContents object.
  virtual WebContents* GetWebContents() = 0;

  virtual void UpdateOverridingUserAgent() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
