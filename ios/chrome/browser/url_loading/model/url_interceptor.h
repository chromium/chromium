// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_INTERCEPTOR_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_INTERCEPTOR_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

// An object that defines the behavior of a URL interception.
// This allows Chrome to capture specific internal requests and execute
// custom logic before the standard url-opening flow begins.
class URLInterceptor {
 public:
  URLInterceptor() = default;
  virtual ~URLInterceptor() = default;

  // The logic that executes when a matching URL is intercepted.
  // Returns true if the interception succeeded fully and successfully, or
  // false otherwise.
  virtual bool OnIntercept(const UrlLoadParams& params) = 0;

  // Determines whether the interceptor is currently enabled. When this is
  // `false`, the interceptor is ignored and the interceptor will not run.
  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

  // When `true`, the interceptor automatically switches its active state to
  // `false` after the first successful match. This ensures the logic only
  // triggers once.
  bool deactivates_on_match() const { return deactivates_on_match_; }
  void set_deactivates_on_match(bool deactivates) {
    deactivates_on_match_ = deactivates;
  }

  // Determines if the normal url-opening flow should be stopped after the
  // interception. Setting this to `true` prevents Chrome from loading the URL.
  bool prevent_normal_flow() const { return prevent_normal_flow_; }
  void set_prevent_normal_flow(bool prevent) { prevent_normal_flow_ = prevent; }

 private:
  bool active_ = false;
  bool deactivates_on_match_ = false;
  bool prevent_normal_flow_ = false;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_INTERCEPTOR_H_
