// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_HOST_EVENT_DISPATCHER_H_
#define SERVICES_WS_HOST_EVENT_DISPATCHER_H_

#include "base/component_export.h"
#include "base/macros.h"

namespace ui {
class Event;
}

namespace ws {

// Used to dispatch events. See HostEventQueue for details.
class COMPONENT_EXPORT(WINDOW_SERVICE) HostEventDispatcher {
 public:
  // NOTE: as with other event dispatch related functions, the *caller* owns
  // |event|, but HostEventDispatcher may modify |event| as necessary (but not
  // delete it).
  virtual void DispatchEventFromQueue(ui::Event* event) = 0;

 protected:
  virtual ~HostEventDispatcher() = default;
};

}  // namespace ws

#endif  // SERVICES_WS_HOST_EVENT_DISPATCHER_H_
