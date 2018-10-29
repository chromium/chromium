// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/test_change_tracker.h"

#include <stddef.h>

#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/common/util.h"
#include "services/ws/ids.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace ws {

std::string WindowIdToString(Id id) {
  return (id == 0) ? "null"
                   : base::StringPrintf("%" PRIu32 ",%" PRIu32,
                                        ClientIdFromTransportId(id),
                                        ClientWindowIdFromTransportId(id));
}

namespace {

std::string DirectionToString(mojom::OrderDirection direction) {
  return direction == mojom::OrderDirection::ABOVE ? "above" : "below";
}

std::string OcclusionStateToString(
    const base::Optional<mojom::OcclusionState>& occlusion_state) {
  if (!occlusion_state.has_value()) {
    NOTREACHED();
    return "(null)";
  }

  switch (occlusion_state.value()) {
    case mojom::OcclusionState::kUnknown:
      return "UNKNOWN";
    case mojom::OcclusionState::kVisible:
      return "VISIBLE";
    case mojom::OcclusionState::kOccluded:
      return "OCCLUDED";
    case mojom::OcclusionState::kHidden:
      return "HIDDEN";
  }

  NOTREACHED();
  return "UNKNOWN";
}

enum class ChangeDescriptionType { ONE, TWO };

std::string ChangeToDescription(const Change& change,
                                ChangeDescriptionType type) {
  switch (change.type) {
    case CHANGE_TYPE_EMBED:
      if (type == ChangeDescriptionType::ONE)
        return "OnEmbed";
      return base::StringPrintf("OnEmbed drawn=%s",
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_EMBED_FROM_TOKEN:
      return base::StringPrintf("OnEmbedFromToken");

    case CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED:
      return base::StringPrintf("OnEmbeddedAppDisconnected window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_UNEMBED:
      return base::StringPrintf("OnUnembed window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_CAPTURE_CHANGED:
      return base::StringPrintf("OnCaptureChanged new_window=%s old_window=%s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_FRAME_SINK_ID_ALLOCATED:
      return base::StringPrintf("OnFrameSinkIdAllocated window=%s %s",
                                WindowIdToString(change.window_id).c_str(),
                                change.frame_sink_id.ToString().c_str());

    case CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW:
      return base::StringPrintf("AddTransientWindow parent = %s child = %s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_NODE_BOUNDS_CHANGED:
      return base::StringPrintf(
          "BoundsChanged window=%s old_bounds=%s new_bounds=%s "
          "local_surface_id=%s",
          WindowIdToString(change.window_id).c_str(),
          change.bounds.ToString().c_str(), change.bounds2.ToString().c_str(),
          change.local_surface_id ? change.local_surface_id->ToString().c_str()
                                  : "(none)");

    case CHANGE_TYPE_NODE_HIERARCHY_CHANGED:
      return base::StringPrintf(
          "HierarchyChanged window=%s old_parent=%s new_parent=%s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str(),
          WindowIdToString(change.window_id3).c_str());

    case CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT:
      return base::StringPrintf(
          "RemoveTransientWindowFromParent parent = %s child = %s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_NODE_REORDERED:
      return base::StringPrintf("Reordered window=%s relative=%s direction=%s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str(),
                                DirectionToString(change.direction).c_str());

    case CHANGE_TYPE_NODE_DELETED:
      return base::StringPrintf("WindowDeleted window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_NODE_VISIBILITY_CHANGED:
      return base::StringPrintf("VisibilityChanged window=%s visible=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED:
      return base::StringPrintf("DrawnStateChanged window=%s drawn=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_INPUT_EVENT: {
      std::string result = base::StringPrintf(
          "InputEvent window=%s event_action=%d",
          WindowIdToString(change.window_id).c_str(), change.event_action);
      if (change.matches_event_observer)
        result += " matches_event_observer";
      return result;
    }

    case CHANGE_TYPE_OBSERVED_EVENT:
      return base::StringPrintf("ObservedEvent event_action=%d",
                                change.event_action);

    case CHANGE_TYPE_PROPERTY_CHANGED:
      return base::StringPrintf("PropertyChanged window=%s key=%s value=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.property_key.c_str(),
                                change.property_value.c_str());

    case CHANGE_TYPE_FOCUSED:
      return base::StringPrintf("Focused id=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_CURSOR_CHANGED:
      return base::StringPrintf("CursorChanged id=%s cursor_type=%d",
                                WindowIdToString(change.window_id).c_str(),
                                static_cast<int>(change.cursor_type));
    case CHANGE_TYPE_ON_CHANGE_COMPLETED:
      return base::StringPrintf("ChangeCompleted id=%d success=%s",
                                change.change_id,
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_ON_TOP_LEVEL_CREATED:
      return base::StringPrintf("TopLevelCreated id=%d window_id=%s drawn=%s",
                                change.change_id,
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");
    case CHANGE_TYPE_OPACITY:
      return base::StringPrintf("OpacityChanged window_id=%s opacity=%.2f",
                                WindowIdToString(change.window_id).c_str(),
                                change.float_value);
    case CHANGE_TYPE_REQUEST_CLOSE:
      return "RequestClose";
    case CHANGE_TYPE_TRANSFORM_CHANGED:
      return base::StringPrintf("TransformChanged window_id=%s",
                                WindowIdToString(change.window_id).c_str());
    case CHANGE_TYPE_DISPLAY_CHANGED:
      return base::StringPrintf(
          "DisplayChanged window_id=%s display_id=%s",
          WindowIdToString(change.window_id).c_str(),
          base::NumberToString(change.display_id).c_str());
    case CHANGE_TYPE_DRAG_DROP_START:
      return "DragDropStart";
    case CHANGE_TYPE_DRAG_ENTER:
      return base::StringPrintf("DragEnter window_id=%s",
                                WindowIdToString(change.window_id).c_str());
    case CHANGE_TYPE_DRAG_OVER:
      return base::StringPrintf("DragOver window_id=%s",
                                WindowIdToString(change.window_id).c_str());
    case CHANGE_TYPE_DRAG_LEAVE:
      return base::StringPrintf("DragLeave window_id=%s",
                                WindowIdToString(change.window_id).c_str());
    case CHANGE_TYPE_COMPLETE_DROP:
      return base::StringPrintf("CompleteDrop window_id=%s",
                                WindowIdToString(change.window_id).c_str());
    case CHANGE_TYPE_DRAG_DROP_DONE:
      return "DragDropDone";
    case CHANGE_TYPE_TOPMOST_WINDOW_CHANGED:
      return base::StringPrintf(
          "TopmostWindowChanged window_id=%s window_id2=%s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str());
    case CHANGE_TYPE_ON_PERFORM_DRAG_DROP_COMPLETED:
      return base::StringPrintf(
          "OnPerformDragDropCompleted id=%d success=%s action=%d",
          change.change_id, change.bool_value ? "true" : "false",
          change.drag_drop_action);
    case CHANGE_TYPE_ON_OCCLUSION_STATE_CHANGED:
      return base::StringPrintf(
          "OnOcclusionStateChanged window_id=%s, state=%s",
          WindowIdToString(change.window_id).c_str(),
          OcclusionStateToString(change.occlusion_state).c_str());
  }
  return std::string();
}

std::string SingleChangeToDescriptionImpl(const std::vector<Change>& changes,
                                          ChangeDescriptionType change_type) {
  std::string result;
  for (auto& change : changes) {
    if (!result.empty())
      result += "\n";
    result += ChangeToDescription(change, change_type);
  }
  return result;
}

}  // namespace

std::string ChangeToDescription(const Change& change) {
  return ChangeToDescription(change, ChangeDescriptionType::ONE);
}

std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes) {
  std::vector<std::string> strings(changes.size());
  for (size_t i = 0; i < changes.size(); ++i)
    strings[i] = ChangeToDescription(changes[i], ChangeDescriptionType::ONE);
  return strings;
}

std::string SingleChangeToDescription(const std::vector<Change>& changes) {
  return SingleChangeToDescriptionImpl(changes, ChangeDescriptionType::ONE);
}

std::string SingleChangeToDescription2(const std::vector<Change>& changes) {
  return SingleChangeToDescriptionImpl(changes, ChangeDescriptionType::TWO);
}

std::string SingleWindowDescription(const std::vector<TestWindow>& windows) {
  if (windows.empty())
    return "no windows";
  std::string result;
  for (const TestWindow& window : windows)
    result += window.ToString();
  return result;
}

std::string ChangeWindowDescription(const std::vector<Change>& changes) {
  if (changes.size() != 1)
    return std::string();
  std::vector<std::string> window_strings(changes[0].windows.size());
  for (size_t i = 0; i < changes[0].windows.size(); ++i)
    window_strings[i] = "[" + changes[0].windows[i].ToString() + "]";
  return base::JoinString(window_strings, ",");
}

TestWindow WindowDataToTestWindow(const mojom::WindowDataPtr& data) {
  TestWindow window;
  window.parent_id = data->parent_id;
  window.window_id = data->window_id;
  window.visible = data->visible;
  window.properties = mojo::FlatMapToMap(data->properties);
  window.bounds = data->bounds;
  return window;
}

void WindowDatasToTestWindows(const std::vector<mojom::WindowDataPtr>& data,
                              std::vector<TestWindow>* test_windows) {
  for (size_t i = 0; i < data.size(); ++i)
    test_windows->push_back(WindowDataToTestWindow(data[i]));
}

bool ContainsChange(const std::vector<Change>& changes,
                    const std::string& change_description) {
  for (auto& change : changes) {
    if (base::MatchPattern(ChangeToDescription(change), change_description))
      return true;
  }
  return false;
}

Change::Change() = default;

Change::Change(const Change& other) = default;

Change::~Change() = default;

TestChangeTracker::TestChangeTracker() : delegate_(nullptr) {}

TestChangeTracker::~TestChangeTracker() = default;

void TestChangeTracker::OnEmbed(mojom::WindowDataPtr root, bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_EMBED;
  change.bool_value = drawn;
  change.windows.push_back(WindowDataToTestWindow(root));
  AddChange(change);
}

void TestChangeTracker::OnEmbedFromToken(
    mojom::WindowDataPtr root,
    int64_t display_id,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  Change change;
  change.type = CHANGE_TYPE_EMBED_FROM_TOKEN;
  change.display_id = display_id;
  change.windows.push_back(WindowDataToTestWindow(root));
  AddChange(change);
}

void TestChangeTracker::OnEmbeddedAppDisconnected(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowBoundsChanged(
    Id window_id,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_BOUNDS_CHANGED;
  change.window_id = window_id;
  change.bounds = old_bounds;
  change.bounds2 = new_bounds;
  change.local_surface_id = local_surface_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowTransformChanged(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_TRANSFORM_CHANGED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnUnembed(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_UNEMBED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnTransientWindowAdded(Id window_id,
                                               Id transient_window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW;
  change.window_id = window_id;
  change.window_id2 = transient_window_id;
  AddChange(change);
}

void TestChangeTracker::OnTransientWindowRemoved(Id window_id,
                                                 Id transient_window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT;
  change.window_id = window_id;
  change.window_id2 = transient_window_id;
  AddChange(change);
}

void TestChangeTracker::OnCaptureChanged(Id new_capture_window_id,
                                         Id old_capture_window_id) {
  Change change;
  change.type = CHANGE_TYPE_CAPTURE_CHANGED;
  change.window_id = new_capture_window_id;
  change.window_id2 = old_capture_window_id;
  AddChange(change);
}

void TestChangeTracker::OnFrameSinkIdAllocated(
    Id window_id,
    const viz::FrameSinkId& frame_sink_id) {
  Change change;
  change.type = CHANGE_TYPE_FRAME_SINK_ID_ALLOCATED;
  change.window_id = window_id;
  change.frame_sink_id = frame_sink_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowHierarchyChanged(
    Id window_id,
    Id old_parent_id,
    Id new_parent_id,
    std::vector<mojom::WindowDataPtr> windows) {
  Change change;
  change.type = CHANGE_TYPE_NODE_HIERARCHY_CHANGED;
  change.window_id = window_id;
  change.window_id2 = old_parent_id;
  change.window_id3 = new_parent_id;
  WindowDatasToTestWindows(windows, &change.windows);
  AddChange(change);
}

void TestChangeTracker::OnWindowReordered(Id window_id,
                                          Id relative_window_id,
                                          mojom::OrderDirection direction) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REORDERED;
  change.window_id = window_id;
  change.window_id2 = relative_window_id;
  change.direction = direction;
  AddChange(change);
}

void TestChangeTracker::OnWindowDeleted(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DELETED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowVisibilityChanged(Id window_id, bool visible) {
  Change change;
  change.type = CHANGE_TYPE_NODE_VISIBILITY_CHANGED;
  change.window_id = window_id;
  change.bool_value = visible;
  AddChange(change);
}

void TestChangeTracker::OnWindowOpacityChanged(Id window_id, float opacity) {
  Change change;
  change.type = CHANGE_TYPE_OPACITY;
  change.window_id = window_id;
  change.float_value = opacity;
  AddChange(change);
}

void TestChangeTracker::OnWindowDisplayChanged(Id window_id,
                                               int64_t display_id) {
  Change change;
  change.type = CHANGE_TYPE_DISPLAY_CHANGED;
  change.window_id = window_id;
  change.display_id = display_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowParentDrawnStateChanged(Id window_id,
                                                        bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED;
  change.window_id = window_id;
  change.bool_value = drawn;
  AddChange(change);
}

void TestChangeTracker::OnWindowInputEvent(Id window_id,
                                           const ui::Event& event,
                                           int64_t display_id,
                                           bool matches_event_observer) {
  Change change;
  change.type = CHANGE_TYPE_INPUT_EVENT;
  change.window_id = window_id;
  change.event_action = static_cast<int32_t>(event.type());
  change.matches_event_observer = matches_event_observer;
  change.display_id = display_id;
  if (event.IsLocatedEvent())
    change.location1 = event.AsLocatedEvent()->root_location();
  if (event.IsKeyEvent() && event.AsKeyEvent()->properties())
    change.key_event_properties = *event.AsKeyEvent()->properties();
  AddChange(change);
}

void TestChangeTracker::OnObservedInputEvent(const ui::Event& event) {
  Change change;
  change.type = CHANGE_TYPE_OBSERVED_EVENT;
  change.event_action = static_cast<int32_t>(event.type());
  AddChange(change);
}

void TestChangeTracker::OnWindowSharedPropertyChanged(
    Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& data) {
  Change change;
  change.type = CHANGE_TYPE_PROPERTY_CHANGED;
  change.window_id = window_id;
  change.property_key = name;
  if (!data)
    change.property_value = "NULL";
  else
    change.property_value = base::HexEncode(data->data(), data->size());
  AddChange(change);
}

void TestChangeTracker::OnWindowFocused(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_FOCUSED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowCursorChanged(Id window_id,
                                              const ui::CursorData& cursor) {
  Change change;
  change.type = CHANGE_TYPE_CURSOR_CHANGED;
  change.window_id = window_id;
  change.cursor_type = cursor.cursor_type();
  AddChange(change);
}

void TestChangeTracker::OnChangeCompleted(uint32_t change_id, bool success) {
  Change change;
  change.type = CHANGE_TYPE_ON_CHANGE_COMPLETED;
  change.change_id = change_id;
  change.bool_value = success;
  AddChange(change);
}

void TestChangeTracker::OnTopLevelCreated(uint32_t change_id,
                                          mojom::WindowDataPtr window_data,
                                          bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_ON_TOP_LEVEL_CREATED;
  change.change_id = change_id;
  change.window_id = window_data->window_id;
  change.bool_value = drawn;
  AddChange(change);
}

void TestChangeTracker::OnDragDropStart(
    const base::flat_map<std::string, std::vector<uint8_t>>& drag_data) {
  Change change;
  change.type = CHANGE_TYPE_DRAG_DROP_START;
  change.drag_data = drag_data;
  AddChange(change);
}

void TestChangeTracker::OnDragEnter(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_DRAG_ENTER;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnDragOver(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_DRAG_OVER;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnDragLeave(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_DRAG_LEAVE;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnCompleteDrop(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_COMPLETE_DROP;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnDragDropDone() {
  Change change;
  change.type = CHANGE_TYPE_DRAG_DROP_DONE;
  AddChange(change);
}

void TestChangeTracker::OnTopmostWindowChanged(
    const std::vector<Id>& topmost_ids) {
  DCHECK_LE(topmost_ids.size(), 2u);
  Change change;
  change.type = CHANGE_TYPE_TOPMOST_WINDOW_CHANGED;
  change.window_id =
      (topmost_ids.size() > 0) ? topmost_ids[0] : kInvalidTransportId;
  change.window_id2 =
      (topmost_ids.size() > 1) ? topmost_ids[1] : kInvalidTransportId;
  AddChange(change);
}

void TestChangeTracker::OnPerformDragDropCompleted(uint32_t change_id,
                                                   bool success,
                                                   uint32_t action_taken) {
  Change change;
  change.type = CHANGE_TYPE_ON_PERFORM_DRAG_DROP_COMPLETED;
  change.change_id = change_id;
  change.bool_value = success;
  change.drag_drop_action = action_taken;
  AddChange(change);
}

void TestChangeTracker::RequestClose(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_REQUEST_CLOSE;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnOcclusionStateChanged(
    Id window_id,
    mojom::OcclusionState occlusion_state) {
  Change change;
  change.type = CHANGE_TYPE_ON_OCCLUSION_STATE_CHANGED;
  change.window_id = window_id;
  change.occlusion_state = occlusion_state;
  AddChange(change);
}

void TestChangeTracker::AddChange(const Change& change) {
  changes_.push_back(change);
  if (delegate_)
    delegate_->OnChangeAdded();
}

TestWindow::TestWindow() = default;

TestWindow::TestWindow(const TestWindow& other) = default;

TestWindow::~TestWindow() = default;

std::string TestWindow::ToString() const {
  return base::StringPrintf("window=%s parent=%s",
                            WindowIdToString(window_id).c_str(),
                            WindowIdToString(parent_id).c_str());
}

std::string TestWindow::ToString2() const {
  return base::StringPrintf(
      "window=%s parent=%s visible=%s", WindowIdToString(window_id).c_str(),
      WindowIdToString(parent_id).c_str(), visible ? "true" : "false");
}

}  // namespace ws
