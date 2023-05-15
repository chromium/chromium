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

// Generates an NSData blob similar to what WKWebView uses in -interactionState.
// This can be used to use native restore when the cached -interactionState is
// missing (e.g. tab syncing).
//
// See
// https://github.com/WebKit/WebKit/blob/674bd0ec/Source/WebKit/UIProcess/mac/LegacySessionStateCoding.cpp
// for the basis of this implementation.
NSData* SynthesizedSessionRestore(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items,
    bool off_the_record);

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SYNTHESIZED_SESSION_RESTORE_H_
