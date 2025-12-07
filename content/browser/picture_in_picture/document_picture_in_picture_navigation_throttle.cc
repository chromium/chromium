// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/document_picture_in_picture_navigation_throttle.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"

namespace content {

// static
void DocumentPictureInPictureNavigationThrottle::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  // We prevent the main frame of document picture-in-picture windows from doing
  // cross-document navigation.
  NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInOutermostMainFrame() || handle.IsSameDocument() ||
      !handle.GetWebContents() ||
      !handle.GetWebContents()->GetPictureInPictureOptions().has_value()) {
    return;
  }
  // Allow a command-line flag to opt-out of navigation throttling.
  if (base::FeatureList::IsEnabled(
          media::kDocumentPictureInPictureNavigation)) {
    return;
  }
  registry.AddThrottle(
      std::make_unique<DocumentPictureInPictureNavigationThrottle>(
          base::PassKey<DocumentPictureInPictureNavigationThrottle>(),
          registry));
}

DocumentPictureInPictureNavigationThrottle::
    DocumentPictureInPictureNavigationThrottle(
        base::PassKey<DocumentPictureInPictureNavigationThrottle>,
        NavigationThrottleRegistry& registry)
    : NavigationThrottle(registry) {}

DocumentPictureInPictureNavigationThrottle::
    ~DocumentPictureInPictureNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
DocumentPictureInPictureNavigationThrottle::WillStartRequest() {
  return CancelNavigationAndMayClosePiPWindow();
}

NavigationThrottle::ThrottleCheckResult
DocumentPictureInPictureNavigationThrottle::WillCommitWithoutUrlLoader() {
  return CancelNavigationAndMayClosePiPWindow();
}

const char* DocumentPictureInPictureNavigationThrottle::GetNameForLogging() {
  return "DocumentPictureInPictureNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
DocumentPictureInPictureNavigationThrottle::
    CancelNavigationAndMayClosePiPWindow() {
  // Close the PiP window only if this navigation happens in the primary page.
  // Otherwise, just cancel the navigation, that might be for prerendering or
  // some other non-primary pages.
  if (navigation_handle()->IsInPrimaryMainFrame()) {
    // We are not allowed to synchronously close the WebContents here, so we
    // must do it asynchronously.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&WebContents::ClosePage,
                       navigation_handle()->GetWebContents()->GetWeakPtr()));
  }

  return CANCEL_AND_IGNORE;
}

}  // namespace content
