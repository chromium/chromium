// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_CLIENT_CHANGE_H_
#define SERVICES_WS_CLIENT_CHANGE_H_

#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/aura/window_tracker.h"

namespace aura {
class Window;
}

namespace ws {

class ClientChangeTracker;

// Describes the type of the change. Maps to the incoming change from the
// client.
enum class ClientChangeType {
  // Used for WindowTree::SetWindowBounds().
  kBounds,
  // Used for WindowTree::SetCapture() and WindowTree::ReleaseCapture().
  kCapture,
  // Used for WindowTree::SetFocus().
  kFocus,
  // Used for WindowTree::SetWindowProperty().
  kProperty,
  // Used for WindowTree::SetWindowVisibility().
  kVisibility,
};

// ClientChange represents an incoming request from a WindowTreeClient. For
// example, SetWindowBounds() is a request to change the kBounds property of
// the window.
class COMPONENT_EXPORT(WINDOW_SERVICE) ClientChange {
 public:
  // |property_key| is only used for changes of type kProperty.
  ClientChange(ClientChangeTracker* tracker,
               aura::Window* window,
               ClientChangeType type,
               const void* property_key = nullptr);
  ~ClientChange();

  // The window the changes associated with. Is null if the window has been
  // destroyed during processing.
  aura::Window* window() {
    return const_cast<aura::Window*>(
        const_cast<const ClientChange*>(this)->window());
  }

  const aura::Window* window() const {
    return !window_tracker_.windows().empty() ? window_tracker_.windows()[0]
                                              : nullptr;
  }

  ClientChangeType type() const { return type_; }
  const void* property_key() const { return property_key_; }

 private:
  ClientChangeTracker* tracker_;
  aura::WindowTracker window_tracker_;
  const ClientChangeType type_;
  const void* property_key_;

  DISALLOW_COPY_AND_ASSIGN(ClientChange);
};

}  // namespace ws

#endif  // SERVICES_WS_CLIENT_CHANGE_H_
