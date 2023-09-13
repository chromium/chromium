// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_CACHE_COUNTER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_CACHE_COUNTER_H_

#include "base/memory/weak_ptr.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class ChromeBrowserState;

// CacheCounter is a BrowsingDataCounter used to compute the cache size.
class CacheCounter : public browsing_data::BrowsingDataCounter {
 public:
  explicit CacheCounter(ChromeBrowserState* browser_state);

  CacheCounter(const CacheCounter&) = delete;
  CacheCounter& operator=(const CacheCounter&) = delete;

  ~CacheCounter() override;

  // browsing_data::BrowsingDataCounter implementation.
  const char* GetPrefName() const override;
  void Count() override;

 private:
  // Invoked when cache size has been computed.
  void OnCacheSizeCalculated(int64_t cache_size);

  ChromeBrowserState* browser_state_;

  base::WeakPtrFactory<CacheCounter> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_CACHE_COUNTER_H_
