// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/android_accessibility_util.h"

#include <optional>

#include "services/accessibility/android/accessibility_node_info_data_wrapper.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"
#include "services/accessibility/android/test/android_accessibility_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax::android {

using AXEventType = mojom::AccessibilityEventType;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;

TEST(AndroidAccessibilityUtilTest, LiveRegionChangeEvent) {
  auto node_info_data = AXNodeInfoData::New();

  SetProperty(node_info_data.get(), AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(mojom::AccessibilityLiveRegionType::POLITE));

  AccessibilityNodeInfoDataWrapper source_node_info_wrapper(
      nullptr, node_info_data.get());
  EXPECT_EQ(ax::mojom::Event::kLiveRegionChanged,
            ToAXEvent(AXEventType::WINDOW_CONTENT_CHANGED,
                      &source_node_info_wrapper, &source_node_info_wrapper));

  // No events are needed for a node with live region type NONE.
  SetProperty(node_info_data.get(), AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(mojom::AccessibilityLiveRegionType::NONE));

  AccessibilityNodeInfoDataWrapper source_node_info_wrapper_none(
      nullptr, node_info_data.get());
  EXPECT_EQ(std::nullopt, ToAXEvent(AXEventType::WINDOW_CONTENT_CHANGED,
                                    &source_node_info_wrapper,
                                    &source_node_info_wrapper_none));
}

TEST(AndroidAccessibilityUtilTest, ViewSelectedEvent) {
  auto node_info_data = AXNodeInfoData::New();

  AccessibilityNodeInfoDataWrapper source_node_info_wrapper(
      nullptr, node_info_data.get());

  // Normally, a selected event is converted to a focus event.
  EXPECT_EQ(ax::mojom::Event::kFocus,
            ToAXEvent(AXEventType::VIEW_SELECTED, &source_node_info_wrapper,
                      &source_node_info_wrapper));

  // No events are needed for a node with range info.
  node_info_data->range_info = AXRangeInfoData::New();

  EXPECT_EQ(std::nullopt,
            ToAXEvent(AXEventType::VIEW_SELECTED, &source_node_info_wrapper,
                      &source_node_info_wrapper));
}

TEST(AndroidAccessibilityUtilTest, WindowStateChangedEvent) {
  auto node_info_data = AXNodeInfoData::New();

  AccessibilityNodeInfoDataWrapper source_node_info_wrapper(
      nullptr, node_info_data.get());

  // No event type if there's no focused node.
  EXPECT_EQ(std::nullopt, ToAXEvent(AXEventType::WINDOW_STATE_CHANGED,
                                    &source_node_info_wrapper, nullptr));

  // Focused event if there's a focused node.
  EXPECT_EQ(ax::mojom::Event::kFocus,
            ToAXEvent(AXEventType::WINDOW_STATE_CHANGED,
                      &source_node_info_wrapper, &source_node_info_wrapper));
}

}  // namespace ax::android
