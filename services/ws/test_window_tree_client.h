// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_TEST_WINDOW_TREE_CLIENT_H_
#define SERVICES_WS_TEST_WINDOW_TREE_CLIENT_H_

#include <stdint.h>

#include <queue>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#include "services/ws/test_change_tracker.h"
#include "services/ws/test_screen_provider_observer.h"

namespace ws {

class WindowTree;

// WindowTreeClient implementation that logs all changes to a tracker.
class TestWindowTreeClient : public mojom::WindowTreeClient,
                             public TestChangeTracker::Delegate {
 public:
  // Created every time OnWindowInputEvent() is called.
  struct InputEvent {
    InputEvent();
    InputEvent(InputEvent&& other);
    ~InputEvent();

    uint32_t event_id;
    Id window_id;
    int64_t display_id;
    std::unique_ptr<ui::Event> event;
    bool matches_event_observer;
  };

  TestWindowTreeClient();
  ~TestWindowTreeClient() override;

  std::queue<InputEvent>& input_events() { return input_events_; }

  // Returns the oldest InputEvent that was received by OnWindowInputEvent().
  // If no events have been received, |event| in the returned object is null.
  InputEvent PopInputEvent();

  // Removes all InputEvents from |input_events_|.
  void ClearInputEvents();

  std::queue<std::unique_ptr<ui::Event>>& observed_events() {
    return observed_events_;
  }

  // Returns the oldest event received by OnObservedInputEvent(), or null.
  std::unique_ptr<ui::Event> PopObservedEvent();

  // Sets the mojom::WindowTree for this client. Used when creating a client
  // using mojom::WindowTreeFactory.
  void SetWindowTree(mojom::WindowTreePtr tree);

  mojom::WindowTree* tree() { return tree_.get(); }
  TestChangeTracker* tracker() { return &tracker_; }
  Id root_window_id() const { return root_window_id_; }

  // Sets whether changes to the bounds of the root should be tracked. Normally
  // they are ignored (as during startup we often times get random size
  // changes).
  void set_track_root_bounds_changes(bool value) {
    track_root_bounds_changes_ = value;
  }

  // Acks the first InputEvent that was received, and removes it. Returns true
  // if there was an event.
  bool AckFirstEvent(WindowTree* tree, mojom::EventResult result);

  TestScreenProviderObserver* screen_provider_observer() {
    return &screen_provider_observer_;
  }

  // TestChangeTracker::Delegate:
  void OnChangeAdded() override;

  // mojom::WindowTreeClient:
  void OnEmbed(
      mojom::WindowDataPtr root,
      mojom::WindowTreePtr tree,
      int64_t display_id,
      Id focused_window_id,
      bool drawn,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnEmbedFromToken(
      const base::UnguessableToken& token,
      mojom::WindowDataPtr root,
      int64_t display_id,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnEmbeddedAppDisconnected(Id window_id) override;
  void OnUnembed(Id window_id) override;
  void OnCaptureChanged(Id new_capture_window_id,
                        Id old_capture_window_id) override;
  void OnFrameSinkIdAllocated(Id window_id,
                              const viz::FrameSinkId& frame_sink_id) override;
  void OnTopLevelCreated(
      uint32_t change_id,
      mojom::WindowDataPtr data,
      int64_t display_id,
      bool drawn,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnWindowBoundsChanged(
      Id window_id,
      const gfx::Rect& old_bounds,
      const gfx::Rect& new_bounds,
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) override;
  void OnWindowTransformChanged(Id window_id,
                                const gfx::Transform& old_transform,
                                const gfx::Transform& new_transform) override;
  void OnTransientWindowAdded(Id window_id, Id transient_window_id) override;
  void OnTransientWindowRemoved(Id window_id, Id transient_window_id) override;
  void OnWindowHierarchyChanged(
      Id window,
      Id old_parent,
      Id new_parent,
      std::vector<mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction) override;
  void OnWindowDeleted(Id window) override;
  void OnWindowVisibilityChanged(Id window, bool visible) override;
  void OnWindowOpacityChanged(Id window,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowDisplayChanged(Id window_id, int64_t display_id) override;
  void OnWindowParentDrawnStateChanged(Id window, bool drawn) override;
  void OnWindowInputEvent(uint32_t event_id,
                          Id window_id,
                          int64_t display_id,
                          std::unique_ptr<ui::Event> event,
                          bool matches_event_observer) override;
  void OnObservedInputEvent(std::unique_ptr<ui::Event> event) override;
  void OnWindowSharedPropertyChanged(
      Id window,
      const std::string& name,
      const base::Optional<std::vector<uint8_t>>& new_data) override;
  void OnWindowFocused(Id focused_window_id) override;
  void OnWindowCursorChanged(Id window_id, ui::CursorData cursor) override;
  void OnDragDropStart(const base::flat_map<std::string, std::vector<uint8_t>>&
                           drag_data) override;
  void OnDragEnter(Id window,
                   uint32_t key_state,
                   const gfx::Point& position,
                   uint32_t effect_bitmask,
                   OnDragEnterCallback callback) override;
  void OnDragOver(Id window,
                  uint32_t key_state,
                  const gfx::Point& position,
                  uint32_t effect_bitmask,
                  OnDragOverCallback callback) override;
  void OnDragLeave(Id window) override;
  void OnCompleteDrop(Id window,
                      uint32_t key_state,
                      const gfx::Point& position,
                      uint32_t effect_bitmask,
                      OnCompleteDropCallback callback) override;
  void OnPerformDragDropCompleted(uint32_t change_id,
                                  bool success,
                                  uint32_t action_taken) override;
  void OnDragDropDone() override;
  void OnTopmostWindowChanged(const std::vector<Id>& topmost_ids) override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(Id window_id) override;
  void GetScreenProviderObserver(
      mojom::ScreenProviderObserverAssociatedRequest observer) override;
  void OnOcclusionStateChanged(Id window_id,
                               mojom::OcclusionState occlusion_state) override;

 protected:
  TestChangeTracker tracker_;
  mojom::WindowTreePtr tree_;
  Id root_window_id_ = 0;
  bool track_root_bounds_changes_ = false;
  std::queue<InputEvent> input_events_;
  std::queue<std::unique_ptr<ui::Event>> observed_events_;
  TestScreenProviderObserver screen_provider_observer_;
  mojo::AssociatedBinding<mojom::ScreenProviderObserver>
      screen_provider_observer_binding_{&screen_provider_observer_};

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

}  // namespace ws

#endif  // SERVICES_WS_TEST_WINDOW_TREE_CLIENT_H_
