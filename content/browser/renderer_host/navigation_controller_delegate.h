// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_

#include <stdint.h>

#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"

namespace content {

struct LoadCommittedDetails;

// Interface for objects embedding a NavigationController to provide the
// functionality NavigationController needs.
class NavigationControllerDelegate {
 public:
  virtual ~NavigationControllerDelegate() {}

  virtual void NotifyNavigationStateChangedFromController(
      InvalidateTypes changed_flags) = 0;

  // Methods from WebContentsImpl that NavigationControllerImpl needs to
  // call. NavigationControllerImpl cannot call them directly because
  // renderer_host/ cannot depend on WebContents.
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

  virtual void UpdateOverridingUserAgent() = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_CONTROLLER_DELEGATE_H_
