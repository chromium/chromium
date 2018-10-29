// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_INJECTED_EVENT_HANDLER_H_
#define SERVICES_WS_INJECTED_EVENT_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/ws/window_service_observer.h"
#include "ui/aura/window_event_dispatcher_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"

namespace aura {
class WindowTreeHost;
}

namespace ui {

class Event;
}

namespace ws {

class WindowService;

// InjectedEventHandler is responsible for injecting a single event from a
// remote client and notifying a callback when the client acks the event (or
// it is determined the client will never ack the event).
//
// Implementation note: an event may be handled in any of the following
// ways:
// . It may be dispatched to remote client. This is detected by way of
//   OnWillSendEventToClient().
// . It may be handled locally.
// . It may be held and processed later on. This happens if pointer events are
//   being held if WindowEventDispatcher. See
//   WindowEventDispatcher::HoldPointerMoves() for details.
// For the case of the event being sent to a remote client this has to ensure
// if the remote client, or WindowTreeHost is destroyed, then no ack is
// received.
//
// Exported for tests.
class COMPONENT_EXPORT(WINDOW_SERVICE) InjectedEventHandler
    : public aura::WindowEventDispatcherObserver,
      public WindowServiceObserver,
      public aura::WindowObserver,
      public ui::EventHandler {
 public:
  using ResultCallback = base::OnceClosure;

  InjectedEventHandler(WindowService* window_service,
                       aura::WindowTreeHost* window_tree_host);
  ~InjectedEventHandler() override;

  // Injects the event. This should be called only once. This synchronously
  // dispatches the event. If the event is completely processed synchronously,
  // then |result_callback| is run immediately. Otherwise |result_callback| is
  // run when the client acks the event, or it's known that the client will
  // never ack event event. If the InjectedEventHandler is destroyed before the
  // ack, then |result_callback| is never run.
  void Inject(std::unique_ptr<ui::Event> event, ResultCallback result_callback);

 private:
  class ScopedPreTargetRegister;

  // Tracks the client and identifier for an event that this object is waiting
  // on.
  struct EventId {
    // Uniquely identifies the client.
    ClientSpecificId client_id = 0u;

    // Assigned by the WindowTree whose id matches |client_id| for the event.
    uint32_t event_id = 0u;
  };

  // Called when the event has been acked, or it's known an event will never be
  // received.
  void NotifyCallback();

  // Removes the observers installed in the constructor. This is called when
  // its known this object no longer is waiting for the event to be acked.
  void RemoveObservers();

  // aura::WindowEventDispatcherObserver:
  void OnWindowEventDispatcherFinishedProcessingEvent(
      aura::WindowEventDispatcher* dispatcher) override;
  void OnWindowEventDispatcherDispatchedHeldEvents(
      aura::WindowEventDispatcher* dispatcher) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // WindowServiceObserver:
  void OnWillSendEventToClient(ClientSpecificId client_id,
                               uint32_t event_id,
                               const ui::Event& event) override;
  void OnClientAckedEvent(ClientSpecificId client_id,
                          uint32_t event_id) override;
  void OnWillDestroyClient(ClientSpecificId client_id) override;

  // EventHandler:
  void OnEvent(ui::Event* event) override;

  WindowService* window_service_;
  aura::WindowTreeHost* window_tree_host_;
  ResultCallback result_callback_;

  // Non-null if waiting on an ack from a client.
  std::unique_ptr<EventId> event_id_;

  bool event_dispatched_ = false;

  std::unique_ptr<ScopedPreTargetRegister> pre_target_register_;

  base::WeakPtrFactory<InjectedEventHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InjectedEventHandler);
};

}  // namespace ws

#endif  // SERVICES_WS_INJECTED_EVENT_HANDLER_H_
