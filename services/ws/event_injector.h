// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_EVENT_INJECTOR_H_
#define SERVICES_WS_EVENT_INJECTOR_H_

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/public/mojom/event_injector.mojom.h"

namespace ws {

class InjectedEventHandler;
class WindowService;

// See description in mojom for details on this.
class COMPONENT_EXPORT(WINDOW_SERVICE) EventInjector
    : public mojom::EventInjector {
 public:
  explicit EventInjector(WindowService* window_service);
  ~EventInjector() override;

  void AddBinding(mojom::EventInjectorRequest request);

 private:
  struct EventAndHost;
  struct QueuedEvent;
  struct HandlerAndCallback;

  void OnEventDispatched(InjectedEventHandler* handler);

  // Determines the target WindowTreeHost and provides an mapping on |event|
  // before dispatch. If the returned EventAndHost has a null
  // |window_tree_host| there was a problem in conversion and the event should
  // not be dispatched.
  EventAndHost DetermineEventAndHost(int64_t display_id,
                                     std::unique_ptr<ui::Event> event);

  void DispatchNextQueuedEvent();

  // mojom::EventInjector:
  void InjectEvent(int64_t display_id,
                   std::unique_ptr<ui::Event> event,
                   InjectEventCallback cb) override;
  void InjectEventNoAck(int64_t display_id,
                        std::unique_ptr<ui::Event> event) override;

  WindowService* window_service_;

  // Each call to InjectEvent() results in creating and adding a new handler to
  // |handlers_|. The callback in HandlerAndCallback is the callback supplied by
  // the client. The handlers are removed once the event is processed.
  std::vector<std::unique_ptr<HandlerAndCallback>> handlers_;

  mojo::BindingSet<mojom::EventInjector> bindings_;

  base::circular_deque<QueuedEvent> queued_events_;

  DISALLOW_COPY_AND_ASSIGN(EventInjector);
};

}  // namespace ws

#endif  // SERVICES_WS_EVENT_INJECTOR_H_
