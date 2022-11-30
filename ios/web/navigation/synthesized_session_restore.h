// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SYNTHESIZED_SESSION_RESTORE_H_
#define IOS_WEB_NAVIGATION_SYNTHESIZED_SESSION_RESTORE_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "url/gurl.h"

namespace web {

class NavigationItem;
class WebState;

// Class used to generate an NSData blob similar to what WKWebView uses in
// -interactionState. This can be used to use native restore when the otherwise
// cached `interactionState` is unavailable (for example, tab syncing).
// See
// https://github.com/WebKit/WebKit/blob/674bd0ec/Source/WebKit/UIProcess/mac/LegacySessionStateCoding.cpp
// for the basis of this implementation.
class SynthesizedSessionRestore {
 public:
  SynthesizedSessionRestore();
  ~SynthesizedSessionRestore();

  SynthesizedSessionRestore(const SynthesizedSessionRestore&) = delete;
  SynthesizedSessionRestore& operator=(const SynthesizedSessionRestore&) =
      delete;

  // Generate and cache an NSData blob that can be later passed to WKWebView
  // -interactionState.
  void Init(int last_committed_item_index,
            const std::vector<std::unique_ptr<NavigationItem>>& items,
            bool off_the_record);

  // Pass the archived NSData blob to WKWebView via the WebState API. Returns
  // false if restore fails.
  bool Restore(WebState* web_state);

  // Release the cached NSData blob. This is called automatically on Restore,
  // but may also be called if the synthesized data is not used (such as if a
  // cached restore is successful).
  void Clear() { cached_data_ = nil; }

 private:
  // Returns true when running on iOS15 and when the kSynthesizedRestoreSession
  // feature is enabled.
  static bool IsEnabled();

  NSData* cached_data_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SYNTHESIZED_SESSION_RESTORE_H_
