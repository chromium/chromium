// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/automation/automation_api_converters.h"
#include "base/no_destructor.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_event_generator.h"

namespace extensions {

api::automation::MarkerType ConvertMarkerTypeFromAXToAutomation(
    ax::mojom::MarkerType ax) {
  switch (ax) {
    case ax::mojom::MarkerType::kNone:
      return api::automation::MarkerType::kNone;
    case ax::mojom::MarkerType::kSpelling:
      return api::automation::MarkerType::kSpelling;
    case ax::mojom::MarkerType::kGrammar:
      return api::automation::MarkerType::kGrammar;
    case ax::mojom::MarkerType::kTextMatch:
      return api::automation::MarkerType::kTextMatch;
    case ax::mojom::MarkerType::kActiveSuggestion:
      return api::automation::MarkerType::kActiveSuggestion;
    case ax::mojom::MarkerType::kSuggestion:
      return api::automation::MarkerType::kSuggestion;
    case ax::mojom::MarkerType::kHighlight:
      return api::automation::MarkerType::kHighlight;
  }
}

api::automation::TreeChangeType ConvertToAutomationTreeChangeType(
    ax::mojom::Mutation change_type) {
  switch (change_type) {
    case ax::mojom::Mutation::kNone:
      return api::automation::TreeChangeType::kNone;
    case ax::mojom::Mutation::kNodeCreated:
      return api::automation::TreeChangeType::kNodeCreated;
    case ax::mojom::Mutation::kSubtreeCreated:
      return api::automation::TreeChangeType::kSubtreeCreated;
    case ax::mojom::Mutation::kNodeChanged:
      return api::automation::TreeChangeType::kNodeChanged;
    case ax::mojom::Mutation::kTextChanged:
      return api::automation::TreeChangeType::kTextChanged;
    case ax::mojom::Mutation::kNodeRemoved:
      return api::automation::TreeChangeType::kNodeRemoved;
    case ax::mojom::Mutation::kSubtreeUpdateEnd:
      return api::automation::TreeChangeType::kSubtreeUpdateEnd;
  }
}

using AutomationFilter = api::automation::TreeChangeObserverFilter;
ui::TreeChangeObserverFilter ConvertAutomationTreeChangeObserverFilter(
    AutomationFilter filter) {
  switch (filter) {
    case AutomationFilter::kNone:
      return ui::TreeChangeObserverFilter::kNone;
    case AutomationFilter::kNoTreeChanges:
      return ui::TreeChangeObserverFilter::kNoTreeChanges;
    case AutomationFilter::kLiveRegionTreeChanges:
      return ui::TreeChangeObserverFilter::kLiveRegionTreeChanges;
    case AutomationFilter::kTextMarkerChanges:
      return ui::TreeChangeObserverFilter::kTextMarkerChanges;
    case AutomationFilter::kAllTreeChanges:
      return ui::TreeChangeObserverFilter::kAllTreeChanges;
  }
}

// Maps a key, a stringification of values in ui::AXEventGenerator::Event or
// ax::mojom::Event into a value, automation::api::EventType. The runtime
// invariant is that there should be exactly the same number of values in the
// map as is the size of api::automation::EventType.
api::automation::EventType AXEventToAutomationEventType(
    ax::mojom::Event event_type) {
  static base::NoDestructor<std::vector<api::automation::EventType>> enum_map;
  if (enum_map->empty()) {
    for (int i = static_cast<int>(ax::mojom::Event::kMinValue);
         i <= static_cast<int>(ax::mojom::Event::kMaxValue); i++) {
      auto ax_event_type = static_cast<ax::mojom::Event>(i);
      if (ui::ShouldIgnoreAXEventForAutomation(ax_event_type) ||
          ax_event_type == ax::mojom::Event::kNone) {
        enum_map->emplace_back(api::automation::EventType::kNone);
        continue;
      }

      const char* val = ui::ToString(ax_event_type);
      api::automation::EventType automation_event_type =
          api::automation::ParseEventType(val);
      if (automation_event_type == api::automation::EventType::kNone) {
        NOTREACHED_IN_MIGRATION()
            << "Missing mapping from ax::mojom::Event: " << val;
      }

      enum_map->emplace_back(automation_event_type);
    }
  }

  return (*enum_map)[static_cast<int>(event_type)];
}

api::automation::EventType AXGeneratedEventToAutomationEventType(
    ui::AXEventGenerator::Event event_type) {
  static base::NoDestructor<std::vector<api::automation::EventType>> enum_map;
  if (enum_map->empty()) {
    for (int i = 0;
         i <= static_cast<int>(ui::AXEventGenerator::Event::MAX_VALUE); i++) {
      auto ax_event_type = static_cast<ui::AXEventGenerator::Event>(i);
      if (ui::ShouldIgnoreGeneratedEventForAutomation(ax_event_type)) {
        enum_map->emplace_back(api::automation::EventType::kNone);
        continue;
      }

      const char* val = ui::ToString(ax_event_type);
      api::automation::EventType automation_event_type =
          api::automation::ParseEventType(val);
      if (automation_event_type == api::automation::EventType::kNone) {
        NOTREACHED_IN_MIGRATION()
            << "Missing mapping from ui::AXEventGenerator::Event: " << val;
      }

      enum_map->emplace_back(automation_event_type);
    }
  }

  return (*enum_map)[static_cast<int>(event_type)];
}

}  // namespace extensions
