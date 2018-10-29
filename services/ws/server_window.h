// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_SERVER_WINDOW_H_
#define SERVICES_WS_SERVER_WINDOW_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/ws/ids.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
class WindowTargeter;
}  // namespace aura

namespace ui {

class EventHandler;
}

namespace ws {

class DragDropDelegate;
class Embedding;
class WindowTree;

// Tracks any state associated with an aura::Window for the WindowService.
// ServerWindow is created for every window created at the request of a client,
// including the root window of ClientRoots.
class COMPONENT_EXPORT(WINDOW_SERVICE) ServerWindow {
 public:
  ~ServerWindow();

  // Creates a new ServerWindow. The lifetime of the ServerWindow is tied to
  // that of the Window (the Window ends up owning the ServerWindow).
  // |is_top_level| is true if the window represents a top-level window.
  static ServerWindow* Create(aura::Window* window,
                              WindowTree* tree,
                              const viz::FrameSinkId& frame_sink_id,
                              bool is_top_level);

  // Returns the ServerWindow associated with a window, null if not created yet.
  static ServerWindow* GetMayBeNull(aura::Window* window) {
    return const_cast<ServerWindow*>(
        GetMayBeNull(const_cast<const aura::Window*>(window)));
  }
  static const ServerWindow* GetMayBeNull(const aura::Window* window);

  // Explicitly deletes this ServerWindow. This should very rarely be called.
  // The typical use case is ServerWindow is owned by the aura::Window, and
  // deleted when the associated window is deleted.
  void Destroy();

  aura::Window* window() { return window_; }

  WindowTree* owning_window_tree() { return owning_window_tree_; }
  const WindowTree* owning_window_tree() const { return owning_window_tree_; }

  WindowTree* embedded_window_tree();
  const WindowTree* embedded_window_tree() const;

  void set_frame_sink_id(const viz::FrameSinkId& frame_sink_id) {
    frame_sink_id_ = frame_sink_id;
  }
  const viz::FrameSinkId& frame_sink_id() const { return frame_sink_id_; }

  const std::vector<gfx::Rect>& additional_client_areas() const {
    return additional_client_areas_;
  }
  const gfx::Insets& client_area() const { return client_area_; }
  void SetClientArea(const gfx::Insets& insets,
                     const std::vector<gfx::Rect>& additional_client_areas);

  void SetHitTestInsets(const gfx::Insets& mouse, const gfx::Insets& touch);

  void set_attached_frame_sink_id(const viz::FrameSinkId& id) {
    attached_frame_sink_id_ = id;
  }
  const viz::FrameSinkId& attached_frame_sink_id() const {
    return attached_frame_sink_id_;
  }

  void SetCaptureOwner(WindowTree* owner);
  WindowTree* capture_owner() const { return capture_owner_; }

  void set_focus_owner(WindowTree* owner) { focus_owner_ = owner; }
  WindowTree* focus_owner() const { return focus_owner_; }

  // Save |cursor| in |cursor_|. Since this does not update the active cursor,
  // and to avoid confusion, the function is not called set_cursor().
  void StoreCursor(const ui::Cursor& cursor);
  const ui::Cursor& cursor() const { return cursor_; }

  // Returns true if the window has an embedding, and the owning client
  // intercepts events that would normally target descendants.
  bool DoesOwnerInterceptEvents() const;

  // Returns true if this window has a client embedded in it.
  bool HasEmbedding() const { return embedding_.get() != nullptr; }
  void SetEmbedding(std::unique_ptr<Embedding> embedding);
  Embedding* embedding() { return embedding_.get(); }

  // Returns true if the window is a top-level window and there is at least some
  // non-client area.
  bool HasNonClientArea() const;

  bool IsTopLevel() const;

  void AttachCompositorFrameSink(
      viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
      viz::mojom::CompositorFrameSinkClientPtr client);
  bool attached_compositor_frame_sink() const {
    return attached_compositor_frame_sink_;
  }

  void set_local_surface_id(
      const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
    local_surface_id_ = local_surface_id;
  }
  const base::Optional<viz::LocalSurfaceId>& local_surface_id() const {
    return local_surface_id_;
  }

  bool HasDragDropDelegate() const {
    return drag_drop_delegate_.get() != nullptr;
  }
  void SetDragDropDelegate(
      std::unique_ptr<DragDropDelegate> drag_drop_delegate);

  // Returns an id useful for debugging. This returns the id from the client
  // that created the window, otherwise |frame_sink_id_|.
  std::string GetIdForDebugging();

 private:
  friend class ServerWindowTestHelper;

  ServerWindow(aura::Window*,
               WindowTree* tree,
               const viz::FrameSinkId& frame_sink_id,
               bool is_top_level);

  // Forwards to TopLevelEventHandler, see it for details.
  // NOTE: this is only applicable to top-levels.
  bool IsHandlingPointerPressForTesting(ui::PointerId pointer_id);

  aura::Window* window_;

  // Tree that created the window. Null if the window was not created at the
  // request of a client. Generally this is null for first level embedding,
  // otherwise non-null. A first level embedding is one where local code
  // calls InitForEmbed() on a Window not associated with any other clients.
  WindowTree* owning_window_tree_;

  // Non-null if there is an embedding in this window.
  std::unique_ptr<Embedding> embedding_;

  // This is initially the id supplied by the client (for locally created
  // windows it is kWindowServerClientId for the high part and low part an ever
  // increasing number). If the window is used as the embed root, then it
  // changes to high part = id of client being embedded in and low part 0. If
  // used as a top-level, it's changed to the id passed by the client
  // requesting the top-level.
  viz::FrameSinkId frame_sink_id_;

  // Together |client_area_| and |additional_client_areas_| are used to specify
  // the client area. See SetClientArea() in mojom for details.
  gfx::Insets client_area_;
  std::vector<gfx::Rect> additional_client_areas_;

  aura::WindowTargeter* window_targeter_ = nullptr;

  std::unique_ptr<ui::EventHandler> event_handler_;

  // When a window has capture there are two possible clients that can get the
  // events, either the embedder or the embedded client. When |window_| has
  // capture this indicates which client gets the events. If null and |window_|
  // has capture, then events are not sent to a client and not handled by the
  // WindowService (meaning ui/events and aura's event processing continues).
  // For example, a mouse press in the non-client area of a top-level results
  // in views setting capture.
  WindowTree* capture_owner_ = nullptr;

  // This serves the same purpose as |capture_owner_|, but is used for focus.
  // See |capture_owner_| for details.
  WindowTree* focus_owner_ = nullptr;

  base::Optional<viz::LocalSurfaceId> local_surface_id_;

  std::unique_ptr<DragDropDelegate> drag_drop_delegate_;

  // The last cursor that the client has requested. This is only set for embed
  // roots. For top level windows, see WmNativeWidgetAura.
  ui::Cursor cursor_;

  // Set to true once AttachCompositorFrameSink() has been called.
  bool attached_compositor_frame_sink_ = false;

  // FrameSinkId set by way of mojom::WindowTree::AttachFrameSinkId().
  viz::FrameSinkId attached_frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindow);
};

}  // namespace ws

#endif  // SERVICES_WS_SERVER_WINDOW_H_
