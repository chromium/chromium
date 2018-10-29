// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/event_injector.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "services/ws/event_queue.h"
#include "services/ws/injected_event_handler.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace ws {

struct EventInjector::EventAndHost {
  std::unique_ptr<ui::Event> event;
  aura::WindowTreeHost* window_tree_host = nullptr;
};

struct EventInjector::HandlerAndCallback {
  std::unique_ptr<InjectedEventHandler> handler;
  // |callback| is the callback supplied by the client.
  EventInjector::InjectEventCallback callback;
};

struct EventInjector::QueuedEvent {
  int64_t display_id;
  // |callback| is the callback supplied by the client.
  EventInjector::InjectEventCallback callback;
  std::unique_ptr<ui::Event> event;
};

EventInjector::EventInjector(WindowService* window_service)
    : window_service_(window_service) {
  DCHECK(window_service_);
}

EventInjector::~EventInjector() {
  for (auto& handler_and_callback : handlers_)
    std::move(handler_and_callback->callback).Run(false);

  for (auto& queued_event : queued_events_)
    std::move(queued_event.callback).Run(false);
}

void EventInjector::AddBinding(mojom::EventInjectorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void EventInjector::OnEventDispatched(InjectedEventHandler* handler) {
  for (auto iter = handlers_.begin(); iter != handlers_.end(); ++iter) {
    auto& handler_and_callback = *iter;
    if (handler_and_callback->handler.get() == handler) {
      std::move(handler_and_callback->callback).Run(true);
      handlers_.erase(iter);
      return;
    }
  }
  NOTREACHED();
}

EventInjector::EventAndHost EventInjector::DetermineEventAndHost(
    int64_t display_id,
    std::unique_ptr<ui::Event> event) {
  EventAndHost event_and_host;
  aura::WindowTreeHost* window_tree_host =
      window_service_->delegate()->GetWindowTreeHostForDisplayId(display_id);
  if (!window_tree_host) {
    DVLOG(1) << "InjectEvent(): invalid display " << display_id;
    return event_and_host;
  }

  if (event->IsLocatedEvent()) {
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    if (located_event->root_location_f() != located_event->location_f()) {
      DVLOG(1) << "InjectEvent(): root_location and location must match";
      return event_and_host;
    }

    // NOTE: this does not correctly account for coordinates with capture
    // across displays. If needed, the implementation should match something
    // like:
    // https://chromium.googlesource.com/chromium/src/+/ae087c53f5ce4557bfb0b92a13651342336fe18a/services/ws/event_injector.cc#22
  }

  event_and_host.window_tree_host = window_tree_host;
  event_and_host.event = std::move(event);
  return event_and_host;
}

void EventInjector::DispatchNextQueuedEvent() {
  DCHECK(!queued_events_.empty());
  QueuedEvent queued_event = std::move(queued_events_.front());
  queued_events_.pop_front();

  aura::WindowTreeHost* window_tree_host =
      window_service_->delegate()->GetWindowTreeHostForDisplayId(
          queued_event.display_id);
  if (!window_tree_host) {
    std::move(queued_event.callback).Run(false);
    return;
  }

  std::unique_ptr<HandlerAndCallback> handler_and_callback =
      std::make_unique<HandlerAndCallback>();
  handler_and_callback->callback = std::move(queued_event.callback);
  handler_and_callback->handler =
      std::make_unique<InjectedEventHandler>(window_service_, window_tree_host);
  InjectedEventHandler* handler = handler_and_callback->handler.get();
  handlers_.push_back(std::move(handler_and_callback));
  auto callback = base::BindOnce(&EventInjector::OnEventDispatched,
                                 base::Unretained(this), handler);
  handler->Inject(std::move(queued_event.event), std::move(callback));
}

void EventInjector::InjectEvent(int64_t display_id,
                                std::unique_ptr<ui::Event> event,
                                InjectEventCallback cb) {
  EventAndHost event_and_host =
      DetermineEventAndHost(display_id, std::move(event));
  if (!event_and_host.window_tree_host) {
    std::move(cb).Run(false);
    return;
  }

  QueuedEvent queued_event;
  queued_event.display_id = display_id;
  queued_event.callback = std::move(cb);
  queued_event.event = std::move(event_and_host.event);
  queued_events_.push_back(std::move(queued_event));

  // Both EventQueue and |this| are owned by WindowService, so Unretained() is
  // safe.
  window_service_->event_queue()->NotifyWhenReadyToDispatch(base::BindOnce(
      &EventInjector::DispatchNextQueuedEvent, base::Unretained(this)));
}

void EventInjector::InjectEventNoAck(int64_t display_id,
                                     std::unique_ptr<ui::Event> event) {
  EventAndHost event_and_host =
      DetermineEventAndHost(display_id, std::move(event));
  if (!event_and_host.window_tree_host)
    return;

  EventQueue::DispatchOrQueueEvent(window_service_,
                                   event_and_host.window_tree_host,
                                   event_and_host.event.get());
}

}  // namespace ws
