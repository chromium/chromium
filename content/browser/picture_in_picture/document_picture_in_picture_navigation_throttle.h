// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/types/pass_key.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// The DocumentPictureInPictureNavigationThrottle is used to prevent document
// picture-in-picture windows from navigating and closes them when they attempt
// to navigate.
class DocumentPictureInPictureNavigationThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<DocumentPictureInPictureNavigationThrottle>
  MaybeCreateThrottleFor(NavigationHandle* handle);

  DocumentPictureInPictureNavigationThrottle(
      base::PassKey<DocumentPictureInPictureNavigationThrottle>,
      NavigationHandle* handle);
  DocumentPictureInPictureNavigationThrottle(
      const DocumentPictureInPictureNavigationThrottle&) = delete;
  DocumentPictureInPictureNavigationThrottle& operator=(
      const DocumentPictureInPictureNavigationThrottle&) = delete;
  ~DocumentPictureInPictureNavigationThrottle() override;

  // NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillCommitWithoutUrlLoader() override;
  const char* GetNameForLogging() override;

 private:
  ThrottleCheckResult ClosePiPWindowAndCancelNavigation();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_NAVIGATION_THROTTLE_H_
