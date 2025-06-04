// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_TAB_URL_PROVIDER_IMPL_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_TAB_URL_PROVIDER_IMPL_H_

#include <vector>

#import "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/tab_url_provider.h"

namespace base {
class Clock;
class TimeDelta;
}  // namespace base

class BrowserList;
class GURL;

// optimization_guide::TabUrlProvider implementation for iOS.
class TabUrlProviderImpl : public optimization_guide::TabUrlProvider {
 public:
  TabUrlProviderImpl(BrowserList* browser_list, base::Clock* clock);
  ~TabUrlProviderImpl() override;

 private:
  // optimization_guide::TabUrlProvider implementation.
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override;

  // Used to get the URLs in all active tabs. Must out live this class.
  raw_ptr<BrowserList> browser_list_;

  raw_ptr<base::Clock> clock_;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_TAB_URL_PROVIDER_IMPL_H_
