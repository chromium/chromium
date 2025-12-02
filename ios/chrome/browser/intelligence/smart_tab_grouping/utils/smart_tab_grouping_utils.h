// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_SMART_TAB_GROUPING_UTILS_SMART_TAB_GROUPING_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_SMART_TAB_GROUPING_UTILS_SMART_TAB_GROUPING_UTILS_H_

namespace optimization_guide {
namespace proto {
class IosSmartTabGroupingResponse;
}  // namespace proto
}  // namespace optimization_guide

class WebStateList;

// Applies the smart tab group suggestions from the response proto to the
// provided WebStateList.
void ApplySmartTabGroupResponse(
    const optimization_guide::proto::IosSmartTabGroupingResponse&
        response_proto,
    WebStateList* web_state_list);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_SMART_TAB_GROUPING_UTILS_SMART_TAB_GROUPING_UTILS_H_
