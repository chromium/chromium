// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SYNTHESIZED_HISTORY_ENTRY_DATA_H_
#define IOS_WEB_NAVIGATION_SYNTHESIZED_HISTORY_ENTRY_DATA_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "url/gurl.h"

namespace web {

// A helper class used to generate an NSData blob specific to the session
// entry. This is where things like document sequence number, referrer, etc,
// is stored.  Currently only referrer is customizable, but more could be
// added in the future. See
// https://github.com/WebKit/WebKit/blob/674bd0ec/Source/WebKit/UIProcess/mac/LegacySessionStateCoding.cpp
// for more details.
class SynthesizedHistoryEntryData {
 public:
  SynthesizedHistoryEntryData();
  ~SynthesizedHistoryEntryData();
  SynthesizedHistoryEntryData(const SynthesizedHistoryEntryData&) = delete;
  SynthesizedHistoryEntryData& operator=(const SynthesizedHistoryEntryData&) =
      delete;

  // Set the SessionHistoryEntryData referrer value based on the
  // NavigationItem.
  void SetReferrer(GURL referrer) { referrer_ = referrer; }

  // Returns a properly formatted NSData blob to be used for the
  // SessionHistoryEntryData key.
  NSData* AsNSData();

 private:
  // Adds data, all using little-endian.
  void PushBack(const uint8_t* data, size_t size);
  void PushBackGURL(const GURL& url);
  template <typename Type>
  void PushBack(Type value) {
    PushBack(reinterpret_cast<uint8_t*>(&value), sizeof(value));
  }

  std::vector<uint8_t> buffer_;
  GURL referrer_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SYNTHESIZED_HISTORY_ENTRY_DATA_H_
