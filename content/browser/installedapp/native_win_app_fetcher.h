// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_NATIVE_WIN_APP_FETCHER_H_
#define CONTENT_BROWSER_INSTALLEDAPP_NATIVE_WIN_APP_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// This task fetch windows apps related to given url as RelatedApplicationPtrs.
// This class is used as an interface for NativeWinAppFetcherImpl so that it's
// easier to fake for tests.
class CONTENT_EXPORT NativeWinAppFetcher {
 public:
  NativeWinAppFetcher() = default;
  NativeWinAppFetcher(const NativeWinAppFetcher&) = delete;
  NativeWinAppFetcher& operator=(const NativeWinAppFetcher&) = delete;
  virtual ~NativeWinAppFetcher() = default;

  virtual void FetchAppsForUrl(
      const GURL& url,
      base::OnceCallback<void(std::vector<blink::mojom::RelatedApplicationPtr>)>
          callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_NATIVE_WIN_APP_FETCHER_H_
