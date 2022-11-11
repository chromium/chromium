// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_input_event.h"

namespace content {

AttributionInputEvent::AttributionInputEvent() = default;

AttributionInputEvent::~AttributionInputEvent() = default;

AttributionInputEvent::AttributionInputEvent(const AttributionInputEvent&) =
    default;

AttributionInputEvent& AttributionInputEvent::operator=(
    const AttributionInputEvent&) = default;

AttributionInputEvent::AttributionInputEvent(AttributionInputEvent&&) = default;

AttributionInputEvent& AttributionInputEvent::operator=(
    AttributionInputEvent&&) = default;

}  // namespace content
