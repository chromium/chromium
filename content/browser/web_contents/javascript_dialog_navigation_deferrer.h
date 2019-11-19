// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_JAVASCRIPT_DIALOG_NAVIGATION_DEFERRER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_JAVASCRIPT_DIALOG_NAVIGATION_DEFERRER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

// Throttle registered for most navigations in a WebContents, so that they can
// be deferred during a dialog if a JavaScriptDialogNavigationDeferrer (below)
// exists at response time.
class JavaScriptDialogNavigationThrottle
    : public NavigationThrottle,
      public base::SupportsWeakPtr<JavaScriptDialogNavigationThrottle> {
 public:
  // Registers a throttle for most navigations in a tab, unless they target the
  // main frame with a user gesture or will be a download.
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* navigation_handle);
  explicit JavaScriptDialogNavigationThrottle(
      NavigationHandle* navigation_handle);
  ~JavaScriptDialogNavigationThrottle() override = default;

  // NavigationThrottle methods:
  ThrottleCheckResult WillProcessResponse() override;
  void Resume() override;
  const char* GetNameForLogging() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogNavigationThrottle);
};

// Prevents navigations in a WebContents that is showing a modal dialog,
// unless it is a user-initiated main frame navigation (in which case the dialog
// will be auto-dismissed when the navigation completes).
class JavaScriptDialogNavigationDeferrer {
 public:
  JavaScriptDialogNavigationDeferrer();
  ~JavaScriptDialogNavigationDeferrer();

 private:
  friend class JavaScriptDialogNavigationThrottle;

  // Only called by JavaScriptDialogNavigationThrottle::WillProcessResponse, in
  // the case that a dialog is showing at response time.
  void AddThrottle(JavaScriptDialogNavigationThrottle* throttle);

  // Stores a weak reference to a throttle for each deferred navigation.
  std::vector<base::WeakPtr<JavaScriptDialogNavigationThrottle>> throttles_;

  DISALLOW_COPY_AND_ASSIGN(JavaScriptDialogNavigationDeferrer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_JAVASCRIPT_DIALOG_NAVIGATION_DEFERRER_H_
