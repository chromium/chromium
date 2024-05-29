// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_ANDROID_ACCESSIBILITY_UTIL_H_
#define SERVICES_ACCESSIBILITY_ANDROID_ANDROID_ACCESSIBILITY_UTIL_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom-forward.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ax::android {

class AccessibilityInfoDataWrapper;

// Returns Chrome accessibility event type if we need to dispatch some event
// explicitly. Otherwise returns nullopt.
std::optional<ax::mojom::Event> ToAXEvent(
    mojom::AccessibilityEventType android_event_type,
    AccessibilityInfoDataWrapper* source_node,
    AccessibilityInfoDataWrapper* focused_node);

std::optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action);

ax::mojom::Action ConvertToChromeAction(
    const mojom::AccessibilityActionType action);

AccessibilityInfoDataWrapper* GetSelectedNodeInfoFromAdapterViewEvent(
    const mojom::AccessibilityEventData& event_data,
    AccessibilityInfoDataWrapper* source_node);

std::string ToLiveStatusString(mojom::AccessibilityLiveRegionType type);

template <class DataType, class PropType>
bool GetBooleanProperty(DataType* node, PropType prop) {
  if (!node || !node->boolean_properties) {
    return false;
  }

  auto it = node->boolean_properties->find(prop);
  if (it == node->boolean_properties->end()) {
    return false;
  }

  return it->second;
}

template <class PropMTypeMap, class PropType>
bool HasProperty(const PropMTypeMap& properties, const PropType prop) {
  if (!properties) {
    return false;
  }

  return properties->find(prop) != properties->end();
}

template <class PropMTypeMap, class PropType, class OutType>
bool GetProperty(const PropMTypeMap& properties,
                 const PropType prop,
                 OutType* out_value) {
  if (!properties) {
    return false;
  }

  auto it = properties->find(prop);
  if (it == properties->end()) {
    return false;
  }

  *out_value = it->second;
  return true;
}

template <class InfoDataType, class PropType>
bool HasNonEmptyStringProperty(InfoDataType* node, PropType prop) {
  if (!node || !node->string_properties) {
    return false;
  }

  auto it = node->string_properties->find(prop);
  if (it == node->string_properties->end()) {
    return false;
  }

  return !it->second.empty();
}

}  // namespace ax::android

#endif  // SERVICES_ACCESSIBILITY_ANDROID_ANDROID_ACCESSIBILITY_UTIL_H_
