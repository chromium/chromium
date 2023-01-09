// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_HTTP_ERROR_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_HTTP_ERROR_NAVIGATION_THROTTLE_H_

#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {

// If a navigation received a response with a bad HTTP status code, we will
// still display the contents from the response body sent by the site (e.g. some
// sites have custom 404 pages). In cases where the response's body is empty,
// however, we should display an error page if possible (instead of showing a
// blank page, which might confuse users). This throttle will defer main frame
// potentially-empty HTTP error navigations until we can determine if its
// response body is empty or not.
class HttpErrorNavigationThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle& navigation_handle);

  ~HttpErrorNavigationThrottle() override;

 private:
  explicit HttpErrorNavigationThrottle(NavigationHandle& navigation_handle);

  // NavigationThrottle overrides.
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillProcessResponse() override;

  void OnBodyReadable(MojoResult);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::SimpleWatcher body_consumer_watcher_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_HTTP_ERROR_NAVIGATION_THROTTLE_H_
