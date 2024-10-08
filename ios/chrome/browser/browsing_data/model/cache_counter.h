// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_CACHE_COUNTER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_CACHE_COUNTER_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "components/browsing_data/core/counters/browsing_data_counter.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// CacheCounter is a BrowsingDataCounter used to compute the cache size.
class CacheCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit CacheCounter(ProfileIOS* profile);

  CacheCounter(const CacheCounter&) = delete;
  CacheCounter& operator=(const CacheCounter&) = delete;

  ~CacheCounter() override;

  // browsing_data::BrowsingDataCounter implementation.
  const char* GetPrefName() const override;
  void Count() override;

 private:
  // Invoked when cache size has been computed.
  void OnCacheSizeCalculated(int64_t cache_size);

  raw_ptr<ProfileIOS> profile_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_CACHE_COUNTER_H_
