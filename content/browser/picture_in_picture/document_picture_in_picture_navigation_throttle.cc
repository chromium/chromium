// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/document_picture_in_picture_navigation_throttle.h"

#include "base/functional/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace content {

// static
std::unique_ptr<DocumentPictureInPictureNavigationThrottle>
DocumentPictureInPictureNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  // We prevent the main frame of document picture-in-picture windows from doing
  // cross-document navigation.
  if (!handle->IsInMainFrame() || handle->IsSameDocument() ||
      !handle->GetWebContents() ||
      !handle->GetWebContents()->GetPictureInPictureOptions().has_value()) {
    return nullptr;
  }
  return std::make_unique<DocumentPictureInPictureNavigationThrottle>(
      base::PassKey<DocumentPictureInPictureNavigationThrottle>(), handle);
}

DocumentPictureInPictureNavigationThrottle::
    DocumentPictureInPictureNavigationThrottle(
        base::PassKey<DocumentPictureInPictureNavigationThrottle>,
        NavigationHandle* handle)
    : NavigationThrottle(handle) {}

DocumentPictureInPictureNavigationThrottle::
    ~DocumentPictureInPictureNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
DocumentPictureInPictureNavigationThrottle::WillStartRequest() {
  return ClosePiPWindowAndCancelNavigation();
}

NavigationThrottle::ThrottleCheckResult
DocumentPictureInPictureNavigationThrottle::WillCommitWithoutUrlLoader() {
  return ClosePiPWindowAndCancelNavigation();
}

const char* DocumentPictureInPictureNavigationThrottle::GetNameForLogging() {
  return "DocumentPictureInPictureNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
DocumentPictureInPictureNavigationThrottle::
    ClosePiPWindowAndCancelNavigation() {
  // We are not allowed to synchronously close the WebContents here, so we must
  // do it asynchronously.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&WebContents::ClosePage,
                     navigation_handle()->GetWebContents()->GetWeakPtr()));

  return CANCEL_AND_IGNORE;
}

}  // namespace content
