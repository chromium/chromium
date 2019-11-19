// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_TYPE_BUILDER_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_INSPECTOR_TYPE_BUILDER_HELPER_H_

#include "third_party/blink/renderer/core/inspector/protocol/Accessibility.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

using protocol::Accessibility::AXProperty;
using protocol::Accessibility::AXValue;
using protocol::Accessibility::AXValueSource;
namespace AXValueTypeEnum = protocol::Accessibility::AXValueTypeEnum;

std::unique_ptr<AXProperty> CreateProperty(const String& name,
                                           std::unique_ptr<AXValue>);
std::unique_ptr<AXProperty> CreateProperty(IgnoredReason);

std::unique_ptr<AXValue> CreateValue(
    const String& value,
    const String& type = AXValueTypeEnum::String);
std::unique_ptr<AXValue> CreateValue(
    int value,
    const String& type = AXValueTypeEnum::Integer);
std::unique_ptr<AXValue> CreateValue(
    float value,
    const String& value_type = AXValueTypeEnum::Number);
std::unique_ptr<AXValue> CreateBooleanValue(
    bool value,
    const String& value_type = AXValueTypeEnum::Boolean);
std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    const AXObject&,
    String* name = nullptr,
    const String& value_type = AXValueTypeEnum::Idref);
std::unique_ptr<AXValue> CreateRelatedNodeListValue(AXRelatedObjectVector&,
                                                    const String& value_type);
std::unique_ptr<AXValue> CreateRelatedNodeListValue(
    AXObject::AXObjectVector& ax_objects,
    const String& value_type = AXValueTypeEnum::IdrefList);

std::unique_ptr<AXValueSource> CreateValueSource(NameSource&);

}  // namespace blink

#endif  // InspectorAccessibilityAgent_h
