// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_CONVERTERS_H_
#define EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_CONVERTERS_H_

#include "extensions/common/api/automation.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/platform/automation/automation_api_util.h"

namespace extensions {

// Utility functions to convert to and from automation API types.

api::automation::MarkerType ConvertMarkerTypeFromAXToAutomation(
    ax::mojom::MarkerType ax);

api::automation::TreeChangeType ConvertToAutomationTreeChangeType(
    ax::mojom::Mutation change_type);

ui::TreeChangeObserverFilter ConvertAutomationTreeChangeObserverFilter(
    api::automation::TreeChangeObserverFilter filter);

// Maps a key, a stringification of values in ui::AXEventGenerator::Event or
// ax::mojom::Event into a value, automation::api::EventType. The runtime
// invariant is that there should be exactly the same number of values in the
// map as is the size of api::automation::EventType.
api::automation::EventType AXEventToAutomationEventType(
    ax::mojom::Event event_type);

api::automation::EventType AXGeneratedEventToAutomationEventType(
    ui::AXEventGenerator::Event event_type);

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_AUTOMATION_AUTOMATION_API_CONVERTERS_H_
