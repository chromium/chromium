// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TOUCH_SELECTION_CONTROLLER_CLIENT_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_TOUCH_SELECTION_CONTROLLER_CLIENT_MANAGER_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"

namespace gfx {
class Point;
class SelectionBound;
}

namespace ui {
class TouchSelectionController;
class TouchSelectionControllerClient;
class TouchSelectionMenuClient;
}  // namespace ui

namespace content {

// This class defines an interface for a manager class that allows multiple
// TouchSelectionControllerClients to work together with a single
// TouchSelectionController.
class CONTENT_EXPORT TouchSelectionControllerClientManager {
 public:
  virtual ~TouchSelectionControllerClientManager() {}

  virtual void DidStopFlinging() = 0;

  virtual void OnSwipeToMoveCursorBegin() = 0;

  virtual void OnSwipeToMoveCursorEnd() = 0;

  // Used by clients to notify the manager that the client's hit test region has
  // updated, to allow selection bounds to be updated if needed.
  virtual void OnClientHitTestRegionUpdated(
      ui::TouchSelectionControllerClient* client) = 0;

  // The manager uses this class' methods to notify observers about important
  // events.
  class CONTENT_EXPORT Observer : public base::CheckedObserver {
   public:
    // Warns observers the manager is shutting down. The manager's view may not
    // be rigidly defined with respect to the lifetime of the client's views.
    virtual void OnManagerWillDestroy(
        TouchSelectionControllerClientManager* manager) = 0;
  };

  // Clients call this method when their selection bounds change, so that the
  // manager can determine which client should be considered the active client,
  // i.e. receive the selection handles and (possibly) a quickmenu.
  virtual void UpdateClientSelectionBounds(
      const gfx::SelectionBound& start,
      const gfx::SelectionBound& end,
      ui::TouchSelectionControllerClient* client,
      ui::TouchSelectionMenuClient* menu_client) = 0;

  // Used by clients to inform the manager that the client no longer wants to
  // participate in touch selection editing, usually because the client's view
  // is being destroyed or detached.
  virtual void InvalidateClient(ui::TouchSelectionControllerClient* client) = 0;

  // Provides direct access to the TouchSelectionController that will be used
  // with all clients accessing this manager. May return null values on Android.
  virtual ui::TouchSelectionController* GetTouchSelectionController() = 0;

  // The following two functions allow clients (or their owners, etc.) to
  // monitor the manager's lifetime.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Used to request the active client to show a context menu at |location|.
  virtual void ShowContextMenu(const gfx::Point& location) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TOUCH_SELECTION_CONTROLLER_CLIENT_MANAGER_H_
