// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_FAVICON_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_FAVICON_FAVICON_CLIENT_IMPL_H_

#include <vector>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon_base/favicon_callback.h"

class GURL;

// FaviconClientImpl implements the favicon::FaviconClient interface on iOS.
class FaviconClientImpl : public favicon::FaviconClient {
 public:
  FaviconClientImpl();
  ~FaviconClientImpl() override;

 private:
  // favicon::FaviconClient implementation.
  bool IsNativeApplicationURL(const GURL& url) override;
  base::CancelableTaskTracker::TaskId GetFaviconForNativeApplicationURL(
      const GURL& url,
      const std::vector<int>& desired_sizes_in_pixel,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;

  DISALLOW_COPY_AND_ASSIGN(FaviconClientImpl);
};

#endif  // IOS_CHROME_BROWSER_FAVICON_FAVICON_CLIENT_IMPL_H_
