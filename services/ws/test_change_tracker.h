// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TEST_CHANGE_TRACKER_H_
#define SERVICES_WS_TEST_CHANGE_TRACKER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "services/ws/common/types.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

namespace ws {

enum ChangeType {
  CHANGE_TYPE_CAPTURE_CHANGED,
  CHANGE_TYPE_FRAME_SINK_ID_ALLOCATED,
  CHANGE_TYPE_EMBED,
  CHANGE_TYPE_EMBED_FROM_TOKEN,
  CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED,
  CHANGE_TYPE_UNEMBED,
  // TODO(sky): nuke NODE.
  CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW,
  CHANGE_TYPE_NODE_BOUNDS_CHANGED,
  CHANGE_TYPE_NODE_HIERARCHY_CHANGED,
  CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT,
  CHANGE_TYPE_NODE_REORDERED,
  CHANGE_TYPE_NODE_VISIBILITY_CHANGED,
  CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED,
  CHANGE_TYPE_NODE_DELETED,
  CHANGE_TYPE_INPUT_EVENT,
  CHANGE_TYPE_OBSERVED_EVENT,
  CHANGE_TYPE_PROPERTY_CHANGED,
  CHANGE_TYPE_FOCUSED,
  CHANGE_TYPE_CURSOR_CHANGED,
  CHANGE_TYPE_ON_CHANGE_COMPLETED,
  CHANGE_TYPE_ON_TOP_LEVEL_CREATED,
  CHANGE_TYPE_OPACITY,
  CHANGE_TYPE_REQUEST_CLOSE,
  CHANGE_TYPE_TRANSFORM_CHANGED,
  CHANGE_TYPE_DISPLAY_CHANGED,
  CHANGE_TYPE_DRAG_DROP_START,
  CHANGE_TYPE_DRAG_ENTER,
  CHANGE_TYPE_DRAG_OVER,
  CHANGE_TYPE_DRAG_LEAVE,
  CHANGE_TYPE_COMPLETE_DROP,
  CHANGE_TYPE_DRAG_DROP_DONE,
  CHANGE_TYPE_TOPMOST_WINDOW_CHANGED,
  CHANGE_TYPE_ON_PERFORM_DRAG_DROP_COMPLETED,
  CHANGE_TYPE_ON_OCCLUSION_STATE_CHANGED,
};

// TODO(sky): consider nuking and converting directly to WindowData.
struct TestWindow {
  TestWindow();
  TestWindow(const TestWindow& other);
  ~TestWindow();

  // Returns a string description of this.
  std::string ToString() const;

  // Returns a string description that includes visible and drawn.
  std::string ToString2() const;

  Id parent_id = 0;
  Id window_id = 0;
  bool visible = false;
  gfx::Rect bounds;
  std::map<std::string, std::vector<uint8_t>> properties;
};

// Tracks a call to WindowTreeClient. See the individual functions for the
// fields that are used.
struct Change {
  Change();
  Change(const Change& other);
  ~Change();

  ChangeType type = CHANGE_TYPE_EMBED;
  std::vector<TestWindow> windows;
  Id window_id = 0;
  Id window_id2 = 0;
  Id window_id3 = 0;
  gfx::Rect bounds;
  gfx::Rect bounds2;
  viz::FrameSinkId frame_sink_id;
  base::Optional<viz::LocalSurfaceId> local_surface_id;
  int32_t event_action = 0;
  bool matches_event_observer = false;
  std::string embed_url;
  mojom::OrderDirection direction;
  bool bool_value = false;
  float float_value = 0.f;
  std::string property_key;
  std::string property_value;
  ui::CursorType cursor_type = ui::CursorType::kNull;
  uint32_t change_id = 0u;
  gfx::Transform transform;
  // Set in OnWindowInputEvent() if the event is a KeyEvent.
  base::flat_map<std::string, std::vector<uint8_t>> key_event_properties;
  int64_t display_id = 0;
  gfx::Point location1;
  base::flat_map<std::string, std::vector<uint8_t>> drag_data;
  uint32_t drag_drop_action = 0u;
  base::Optional<mojom::OcclusionState> occlusion_state;
};

// The ChangeToDescription related functions convert a Change into a string.
// To avoid updating all tests as more descriptive strings are added, new
// variants are added and identified with a numeric suffix. Differences
// between versions:
// 1 and no suffix is the original version.
// 2: OnEmbed() includes the boolean value supplied to OnEmbed().

std::string ChangeToDescription(const Change& change);

// Converts Changes to string descriptions.
std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes);

// Convenience for returning the description of the first item in |changes|.
// Returns an empty string if |changes| has something other than one entry.
std::string SingleChangeToDescription(const std::vector<Change>& changes);

std::string SingleChangeToDescription2(const std::vector<Change>& changes);

// Convenience for returning the description of the first item in |windows|.
// Returns an empty string if |windows| has something other than one entry.
std::string SingleWindowDescription(const std::vector<TestWindow>& windows);

// Returns a string description of |changes[0].windows|. Returns an empty string
// if change.size() != 1.
std::string ChangeWindowDescription(const std::vector<Change>& changes);

// Converts WindowDatas to TestWindows.
void WindowDatasToTestWindows(const std::vector<mojom::WindowDataPtr>& data,
                              std::vector<TestWindow>* test_windows);

// Returns true if |changes| contains a Change matching |change_description|.
// |change_description| is a pattern which should be compared with
// base::MatchPattern (see base/strings/pattern.h for the details).
bool ContainsChange(const std::vector<Change>& changes,
                    const std::string& change_description);

// TestChangeTracker is used to record WindowTreeClient functions. It notifies
// a delegate any time a change is added.
class TestChangeTracker {
 public:
  // Used to notify the delegate when a change is added. A change corresponds to
  // a single WindowTreeClient function.
  class Delegate {
   public:
    virtual void OnChangeAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  TestChangeTracker();
  ~TestChangeTracker();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Change>* changes() { return &changes_; }

  // Each of these functions generate a Change. There is one per
  // WindowTreeClient function.
  void OnEmbed(mojom::WindowDataPtr root, bool drawn);
  void OnEmbedFromToken(
      mojom::WindowDataPtr root,
      int64_t display_id,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id);
  void OnEmbeddedAppDisconnected(Id window_id);
  void OnUnembed(Id window_id);
  void OnCaptureChanged(Id new_capture_window_id, Id old_capture_window_id);
  void OnFrameSinkIdAllocated(Id window_id,
                              const viz::FrameSinkId& frame_sink_id);
  void OnTransientWindowAdded(Id window_id, Id transient_window_id);
  void OnTransientWindowRemoved(Id window_id, Id transient_window_id);
  void OnWindowBoundsChanged(
      Id window_id,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id);
  void OnWindowTransformChanged(Id window_id);
  void OnWindowHierarchyChanged(Id window_id,
                                Id old_parent_id,
                                Id new_parent_id,
                                std::vector<mojom::WindowDataPtr> windows);
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction);
  void OnWindowDeleted(Id window_id);
  void OnWindowVisibilityChanged(Id window_id, bool visible);
  void OnWindowOpacityChanged(Id window_id, float opacity);
  void OnWindowDisplayChanged(Id window_id, int64_t display_id);
  void OnWindowParentDrawnStateChanged(Id window_id, bool drawn);
  void OnWindowInputEvent(Id window_id,
                          const ui::Event& event,
                          int64_t display_id,
                          bool matches_event_observer);
  void OnObservedInputEvent(const ui::Event& event);
  void OnPointerEventObserved(const ui::Event& event, Id window_id);
  void OnWindowSharedPropertyChanged(
      Id window_id,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& data);
  void OnWindowFocused(Id window_id);
  void OnWindowCursorChanged(Id window_id, const ui::CursorData& cursor);
  void OnChangeCompleted(uint32_t change_id, bool success);
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr window_data,
                         bool drawn);
  void OnDragDropStart(
      const base::flat_map<std::string, std::vector<uint8_t>>& drag_data);
  void OnDragEnter(Id window_id);
  void OnDragOver(Id window_id);
  void OnDragLeave(Id widnow_id);
  void OnCompleteDrop(Id window_id);
  void OnDragDropDone();
  void OnTopmostWindowChanged(const std::vector<Id>& topmost_ids);
  void OnPerformDragDropCompleted(uint32_t change_id,
                                  bool success,
                                  uint32_t action_taken);
  void RequestClose(Id window_id);
  void OnOcclusionStateChanged(Id window_id,
                               mojom::OcclusionState occlusion_state);

 private:
  void AddChange(const Change& change);

  Delegate* delegate_;
  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeTracker);
};

}  // namespace ws

#endif  // SERVICES_WS_TEST_CHANGE_TRACKER_H_
