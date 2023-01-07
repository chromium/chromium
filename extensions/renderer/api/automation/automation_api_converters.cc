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
      return api::automation::MARKER_TYPE_NONE;
    case ax::mojom::MarkerType::kSpelling:
      return api::automation::MARKER_TYPE_SPELLING;
    case ax::mojom::MarkerType::kGrammar:
      return api::automation::MARKER_TYPE_GRAMMAR;
    case ax::mojom::MarkerType::kTextMatch:
      return api::automation::MARKER_TYPE_TEXTMATCH;
    case ax::mojom::MarkerType::kActiveSuggestion:
      return api::automation::MARKER_TYPE_ACTIVESUGGESTION;
    case ax::mojom::MarkerType::kSuggestion:
      return api::automation::MARKER_TYPE_SUGGESTION;
    case ax::mojom::MarkerType::kHighlight:
      return api::automation::MARKER_TYPE_HIGHLIGHT;
  }
}

api::automation::TreeChangeType ConvertToAutomationTreeChangeType(
    ax::mojom::Mutation change_type) {
  switch (change_type) {
    case ax::mojom::Mutation::kNone:
      return api::automation::TREE_CHANGE_TYPE_NONE;
    case ax::mojom::Mutation::kNodeCreated:
      return api::automation::TREE_CHANGE_TYPE_NODECREATED;
    case ax::mojom::Mutation::kSubtreeCreated:
      return api::automation::TREE_CHANGE_TYPE_SUBTREECREATED;
    case ax::mojom::Mutation::kNodeChanged:
      return api::automation::TREE_CHANGE_TYPE_NODECHANGED;
    case ax::mojom::Mutation::kTextChanged:
      return api::automation::TREE_CHANGE_TYPE_TEXTCHANGED;
    case ax::mojom::Mutation::kNodeRemoved:
      return api::automation::TREE_CHANGE_TYPE_NODEREMOVED;
    case ax::mojom::Mutation::kSubtreeUpdateEnd:
      return api::automation::TREE_CHANGE_TYPE_SUBTREEUPDATEEND;
  }
}

using AutomationFilter = api::automation::TreeChangeObserverFilter;
ui::TreeChangeObserverFilter ConvertAutomationTreeChangeObserverFilter(
    AutomationFilter filter) {
  switch (filter) {
    case AutomationFilter::TREE_CHANGE_OBSERVER_FILTER_NONE:
      return ui::TreeChangeObserverFilter::kNone;
    case AutomationFilter::TREE_CHANGE_OBSERVER_FILTER_NOTREECHANGES:
      return ui::TreeChangeObserverFilter::kNoTreeChanges;
    case AutomationFilter::TREE_CHANGE_OBSERVER_FILTER_LIVEREGIONTREECHANGES:
      return ui::TreeChangeObserverFilter::kLiveRegionTreeChanges;
    case AutomationFilter::TREE_CHANGE_OBSERVER_FILTER_TEXTMARKERCHANGES:
      return ui::TreeChangeObserverFilter::kTextMarkerChanges;
    case AutomationFilter::TREE_CHANGE_OBSERVER_FILTER_ALLTREECHANGES:
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
        enum_map->emplace_back(api::automation::EVENT_TYPE_NONE);
        continue;
      }

      const char* val = ui::ToString(ax_event_type);
      api::automation::EventType automation_event_type =
          api::automation::ParseEventType(val);
      if (automation_event_type == api::automation::EVENT_TYPE_NONE)
        NOTREACHED() << "Missing mapping from ax::mojom::Event: " << val;

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
        enum_map->emplace_back(api::automation::EVENT_TYPE_NONE);
        continue;
      }

      const char* val = ui::ToString(ax_event_type);
      api::automation::EventType automation_event_type =
          api::automation::ParseEventType(val);
      if (automation_event_type == api::automation::EVENT_TYPE_NONE)
        NOTREACHED() << "Missing mapping from ui::AXEventGenerator::Event: "
                     << val;

      enum_map->emplace_back(automation_event_type);
    }
  }

  return (*enum_map)[static_cast<int>(event_type)];
}

}  // namespace extensions
