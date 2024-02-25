// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_CLIENT_IMPL_H_
#define IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_CLIENT_IMPL_H_

#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon_base/favicon_callback.h"

class GURL;

// FaviconClientImpl implements the favicon::FaviconClient interface on iOS.
class FaviconClientImpl : public favicon::FaviconClient {
 public:
  FaviconClientImpl();

  FaviconClientImpl(const FaviconClientImpl&) = delete;
  FaviconClientImpl& operator=(const FaviconClientImpl&) = delete;

  ~FaviconClientImpl() override;

 private:
  // favicon::FaviconClient implementation.
  bool IsNativeApplicationURL(const GURL& url) override;
  bool IsReaderModeURL(const GURL& url) override;
  const GURL GetOriginalUrlFromReaderModeUrl(const GURL& url) override;
  base::CancelableTaskTracker::TaskId GetFaviconForNativeApplicationURL(
      const GURL& url,
      const std::vector<int>& desired_sizes_in_pixel,
      favicon_base::FaviconResultsCallback callback,
      base::CancelableTaskTracker* tracker) override;
};

#endif  // IOS_CHROME_BROWSER_FAVICON_MODEL_FAVICON_CLIENT_IMPL_H_
