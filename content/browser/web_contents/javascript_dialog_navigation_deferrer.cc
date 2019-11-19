// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/javascript_dialog_navigation_deferrer.h"

#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// static
std::unique_ptr<NavigationThrottle>
JavaScriptDialogNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  // Don't prevent the user from navigating away from the page.
  if (navigation_handle->IsInMainFrame() &&
      (!navigation_handle->IsRendererInitiated() ||
       navigation_handle->HasUserGesture())) {
    return nullptr;
  }

  return std::make_unique<JavaScriptDialogNavigationThrottle>(
      navigation_handle);
}

JavaScriptDialogNavigationThrottle::JavaScriptDialogNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

NavigationThrottle::ThrottleCheckResult
JavaScriptDialogNavigationThrottle::WillProcessResponse() {
  // Don't defer downloads, which don't leave the page.
  if (navigation_handle()->IsDownload())
    return PROCEED;

  JavaScriptDialogNavigationDeferrer* deferrer =
      static_cast<WebContentsImpl*>(navigation_handle()->GetWebContents())
          ->GetJavaScriptDialogNavigationDeferrer();
  if (!deferrer)
    return PROCEED;

  deferrer->AddThrottle(this);
  return DEFER;
}

void JavaScriptDialogNavigationThrottle::Resume() {
  NavigationThrottle::Resume();
}

const char* JavaScriptDialogNavigationThrottle::GetNameForLogging() {
  return "JavaScriptDialogNavigationThrottle";
}

JavaScriptDialogNavigationDeferrer::JavaScriptDialogNavigationDeferrer() {}

JavaScriptDialogNavigationDeferrer::~JavaScriptDialogNavigationDeferrer() {
  for (auto& throttle : throttles_) {
    if (throttle)
      throttle->Resume();
  }
}

void JavaScriptDialogNavigationDeferrer::AddThrottle(
    JavaScriptDialogNavigationThrottle* throttle) {
  throttles_.push_back(throttle->AsWeakPtr());
}

}  // namespace content
