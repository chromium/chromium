// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_NAVIGATION_INFO_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_NAVIGATION_INFO_H_

#include <memory>

#include "url/gurl.h"

namespace content {

class BundledExchangesSource;

// A class that holds necessary information for navigation in a
// BundledExchanges.
class BundledExchangesNavigationInfo {
 public:
  BundledExchangesNavigationInfo(std::unique_ptr<BundledExchangesSource> source,
                                 const GURL& target_inner_url);
  ~BundledExchangesNavigationInfo();

  const BundledExchangesSource& source() const;
  const GURL& target_inner_url() const;

  std::unique_ptr<BundledExchangesNavigationInfo> Clone() const;

 private:
  std::unique_ptr<BundledExchangesSource> source_;
  const GURL target_inner_url_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesNavigationInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_NAVIGATION_INFO_H_
