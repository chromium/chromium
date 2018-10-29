// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/window_tree.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/map.h"
#include "services/ws/client_change.h"
#include "services/ws/client_change_tracker.h"
#include "services/ws/client_root.h"
#include "services/ws/common/util.h"
#include "services/ws/drag_drop_delegate.h"
#include "services/ws/embedding.h"
#include "services/ws/event_observer_helper.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/server_window.h"
#include "services/ws/topmost_window_observer.h"
#include "services/ws/window_delegate_impl.h"
#include "services/ws/window_manager_interface.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_delegate.h"
#include "services/ws/window_service_observer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/os_exchange_data_provider_mus.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/mojo/event_struct_traits.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ws {
namespace {

// Max number of event we let a client queue before pruning. In general only
// bad (or buggy) clients should hit this cap.
#if defined(NDEBUG)
constexpr size_t kMaxQueuedEvents = 100;
#else
constexpr size_t kMaxQueuedEvents = 1000;
#endif

uint32_t GenerateEventAckId() {
  // We do not want to create a sequential id for each event, because that can
  // leak some information to the client. So instead, manufacture the id
  // randomly.
  return 0x1000000 | (rand() & 0xffffff);
}

gfx::Insets MakeInsetsPositive(const gfx::Insets& insets) {
  return gfx::Insets(std::max(0, insets.top()), std::max(0, insets.left()),
                     std::max(0, insets.bottom()), std::max(0, insets.right()));
}

}  // namespace

// Used to track events sent to the client.
struct WindowTree::InFlightEvent {
  // Unique identifier for the event. It is expected that the client respond
  // with this id.
  uint32_t id;

  // Only used for KeyEvents sent to the client. If a KeyEvent is not handled
  // by the client, it's processed locally for accelerators.
  std::unique_ptr<ui::Event> event;
};

WindowTree::KnownWindow::KnownWindow() = default;
WindowTree::KnownWindow::~KnownWindow() = default;

WindowTree::WindowTree(WindowService* window_service,
                       ClientSpecificId client_id,
                       mojom::WindowTreeClient* client,
                       const std::string& client_name)
    : window_service_(window_service),
      client_id_(client_id),
      client_name_(client_name),
      window_tree_client_(client),
      property_change_tracker_(std::make_unique<ClientChangeTracker>()) {
  wm::CaptureController::Get()->AddObserver(this);
}

WindowTree::~WindowTree() {
  wm::CaptureController::Get()->RemoveObserver(this);

  // Delete the embeddings first, that way we don't attempt to notify the client
  // when the windows the client created are deleted.
  while (!client_roots_.empty()) {
    DeleteClientRoot(client_roots_.begin()->get(),
                     DeleteClientRootReason::kDestructor);
  }

  while (FindFirstClientCreatedWindow()) {
    // RemoveWindowFromKnownWindows() should make it such that the Window is no
    // longer recognized as being created (owned) by this client.
    const bool delete_if_owned = true;
    RemoveWindowFromKnownWindows(FindFirstClientCreatedWindow(),
                                 delete_if_owned);
  }

  window_service_->OnWillDestroyWindowTree(this);
}

void WindowTree::InitForEmbed(aura::Window* root,
                              mojom::WindowTreePtr window_tree_ptr) {
  // Force ServerWindow to be created for |root|.
  ServerWindow* server_window =
      window_service_->GetServerWindowForWindowCreateIfNecessary(root);
  const ClientWindowId client_window_id = server_window->frame_sink_id();
  AddWindowToKnownWindows(root, client_window_id, nullptr);
  const bool is_top_level = false;
  ClientRoot* client_root = CreateClientRoot(root, is_top_level);

  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root).id();
  const ClientWindowId focused_window_id =
      root->HasFocus() ? ClientWindowIdForWindow(root) : ClientWindowId();
  const bool drawn = root->IsVisible() && root->GetHost();
  window_tree_client_->OnEmbed(WindowToWindowData(root),
                               std::move(window_tree_ptr), display_id,
                               ClientWindowIdToTransportId(focused_window_id),
                               drawn, server_window->local_surface_id());

  // Reset the frame sink id locally (after calling OnEmbed()). This is
  // needed so that the id used by the client matches the id used locally.
  server_window->set_frame_sink_id(ClientWindowId(client_id_, 0));

  client_root->RegisterVizEmbeddingSupport();
}

void WindowTree::InitFromFactory() {
  connection_type_ = ConnectionType::kOther;
}

void WindowTree::SendEventToClient(aura::Window* window,
                                   const ui::Event& event) {
  // As gesture recognition runs in the client, GestureEvents should not be
  // forwarded. ServerWindow's event processing should ensure no GestureEvents
  // are sent.
  DCHECK(!event.IsGestureEvent());

  const uint32_t event_id = GenerateEventAckId();
  auto* in_flight_event_queue =
      event.IsKeyEvent() ? &in_flight_key_events_ : &in_flight_other_events_;
  if (in_flight_event_queue->size() < kMaxQueuedEvents) {
    std::unique_ptr<InFlightEvent> in_flight_event =
        std::make_unique<InFlightEvent>();
    in_flight_event->id = event_id;
    if (event.type() == ui::ET_KEY_PRESSED ||
        event.type() == ui::ET_KEY_RELEASED) {
      in_flight_event->event = ui::Event::Clone(event);
    }
    in_flight_event_queue->push(std::move(in_flight_event));
  } else {
    DVLOG(1) << "client not responding to events in a timely manner, "
             << "dropping event";
  }

  // Events should only come to windows connected to displays.
  DCHECK(window->GetHost());
  const int64_t display_id = window->GetHost()->GetDisplayId();
  const bool matches_event_observer =
      event_observer_helper_ && event_observer_helper_->DoesEventMatch(event);
  if (event_observer_helper_)
    event_observer_helper_->ClearPendingEvent();

  for (WindowServiceObserver& observer : window_service_->observers())
    observer.OnWillSendEventToClient(client_id_, event_id, event);

  std::unique_ptr<ui::Event> event_to_send = ui::Event::Clone(event);
  if (event.IsLocatedEvent()) {
    // Translate the root location for located events. Event's root location
    // should be in the coordinate of the root window, however the root for the
    // target window in the client can be different from the one in the server,
    // thus the root location needs to be converted from the original coordinate
    // to the one used in the client. See also 'WindowTreeTest.EventLocation'
    // test case.
    ClientRoot* client_root = FindClientRootContaining(window);
    // The |client_root| may have been removed on shutdown.
    if (client_root) {
      gfx::PointF root_location =
          event_to_send->AsLocatedEvent()->root_location_f();
      aura::Window::ConvertPointToTarget(window->GetRootWindow(),
                                         client_root->window(), &root_location);
      event_to_send->AsLocatedEvent()->set_root_location_f(root_location);
    }
  }
  DVLOG(4) << "SendEventToClient window="
           << ServerWindow::GetMayBeNull(window)->GetIdForDebugging()
           << " event_type=" << ui::EventTypeName(event.type())
           << " event_id=" << event_id;
  window_tree_client_->OnWindowInputEvent(
      event_id, TransportIdForWindow(window), display_id,
      std::move(event_to_send), matches_event_observer);
}

void WindowTree::SendObservedEventToClient(int64_t display_id,
                                           std::unique_ptr<ui::Event> event) {
  if (event->IsLocatedEvent()) {
    // Send event locations in screen coordinates, since the client will have no
    // knowledge of the event's target window.
    ui::LocatedEvent* located_event = event->AsLocatedEvent();
    gfx::PointF location = located_event->root_location_f();
    display::Display display;
    if (located_event->target()) {
      location = located_event->target()->GetScreenLocationF(*located_event);
    } else if (display::Screen::GetScreen()->GetDisplayWithDisplayId(
                   display_id, &display)) {
      location += display.bounds().OffsetFromOrigin();
    }
    located_event->set_location_f(location);
    located_event->set_root_location_f(location);
  }
  DVLOG(4) << "SendObservedEventToClient event_type="
           << ui::EventTypeName(event->type());
  window_tree_client_->OnObservedInputEvent(std::move(event));
}

bool WindowTree::IsTopLevel(aura::Window* window) {
  auto iter = FindClientRootWithRoot(window);
  return iter != client_roots_.end() && (*iter)->is_top_level();
}

aura::Window* WindowTree::GetWindowByTransportId(Id transport_window_id) {
  return GetWindowByClientId(MakeClientWindowId(transport_window_id));
}

void WindowTree::RequestClose(ServerWindow* window) {
  DCHECK(window->IsTopLevel());
  DCHECK_EQ(this, window->owning_window_tree());
  window_tree_client_->RequestClose(TransportIdForWindow(window->window()));
}

void WindowTree::OnEmbeddingDestroyed(Embedding* embedding) {
  auto iter = FindClientRootWithRoot(embedding->window());
  DCHECK(iter != client_roots_.end());
  window_tree_client_->OnWindowDeleted(
      TransportIdForWindow(embedding->window()));
  DeleteClientRoot(iter->get(), DeleteClientRootReason::kDeleted);
}

ClientWindowId WindowTree::RemoveScheduledEmbedUsingExistingClient(
    const base::UnguessableToken& embed_token) {
  auto iter = scheduled_embeds_for_existing_client_.find(embed_token);
  if (iter == scheduled_embeds_for_existing_client_.end())
    return ClientWindowId();

  const ClientWindowId client_window_id = MakeClientWindowId(iter->second);
  if (!IsValidIdForNewWindow(client_window_id)) {
    DVLOG(1) << "EmbedUsingToken failed (access denied)";
    return ClientWindowId();
  }
  return client_window_id;
}

void WindowTree::CompleteScheduleEmbedForExistingClient(
    aura::Window* window,
    const ClientWindowId& id,
    const base::UnguessableToken& token) {
  AddWindowToKnownWindows(window, id, nullptr);
  const bool is_top_level = false;
  ClientRoot* client_root = CreateClientRoot(window, is_top_level);

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  // It's expected we only get here if a ServerWindow exists for |window|.
  DCHECK(server_window);

  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
  window_tree_client_->OnEmbedFromToken(token, WindowToWindowData(window),
                                        display_id,
                                        server_window->local_surface_id());

  // Reset the frame sink id locally (after calling OnEmbedFromToken()). This is
  // needed so that the id used by the client matches the id used locally.
  server_window->set_frame_sink_id(id);

  client_root->RegisterVizEmbeddingSupport();
}

bool WindowTree::HasAtLeastOneRootWithCompositorFrameSink() {
  for (auto& client_root : client_roots_) {
    if (ServerWindow::GetMayBeNull(client_root->window())
            ->attached_compositor_frame_sink()) {
      return true;
    }
  }
  return false;
}

bool WindowTree::IsWindowKnown(aura::Window* window) const {
  return window && known_windows_map_.count(window) > 0u;
}

ClientWindowId WindowTree::ClientWindowIdForWindow(aura::Window* window) const {
  auto iter = known_windows_map_.find(window);
  return iter == known_windows_map_.end() ? ClientWindowId()
                                          : iter->second.client_window_id;
}

ClientRoot* WindowTree::GetClientRootForWindow(aura::Window* window) {
  auto iter = FindClientRootWithRoot(window);
  return iter == client_roots_.end() ? nullptr : iter->get();
}

ClientRoot* WindowTree::CreateClientRoot(aura::Window* window,
                                         bool is_top_level) {
  DCHECK(window);

  // Only one client may be embedded in a window at a time.
  ServerWindow* server_window =
      window_service_->GetServerWindowForWindowCreateIfNecessary(window);
  if (server_window->embedded_window_tree()) {
    server_window->embedded_window_tree()->DeleteClientRootWithRoot(window);
    DCHECK(!server_window->embedded_window_tree());
  }

  // Because a new client is being embedded all existing children are removed.
  // This is because this client is no longer able to add children to |window|
  // (until the embedding is removed).
  while (!window->children().empty())
    window->RemoveChild(window->children().front());

  client_roots_.push_back(
      std::make_unique<ClientRoot>(this, window, is_top_level));
  return client_roots_.back().get();
}

void WindowTree::DeleteClientRoot(ClientRoot* client_root,
                                  DeleteClientRootReason reason) {
  aura::Window* window = client_root->window();

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  client_root->UnattachChildFrameSinkIdRecursive(server_window);
  if (server_window->capture_owner() == this) {
    // This client will no longer know about |window|, so it should not receive
    // any events sent to the client.
    server_window->SetCaptureOwner(nullptr);
  }

  // Delete the ClientRoot first, so that we don't attempt to spam the
  // client with a bunch of notifications.
  auto iter = FindClientRootWithRoot(client_root->window());
  DCHECK(iter != client_roots_.end());
  client_roots_.erase(iter);
  client_root = nullptr;  // |client_root| has been deleted.

  const Id client_window_id = TransportIdForWindow(window);

  if (reason == DeleteClientRootReason::kEmbed) {
    // This case happens when another client is embedded in the window this
    // client is embedded in. A window can have at most one client embedded in
    // it. Inform the client of this by way of OnUnembed() and OnWindowDeleted()
    // because the window is no longer known to this client.
    //
    // TODO(sky): consider simplifying this case and just deleting |this|. This
    // is because at this point the client can't do anything useful (the client
    // is unable to reattach to a Window in a display at this point).
    window_tree_client_->OnUnembed(client_window_id);
    window_tree_client_->OnWindowDeleted(client_window_id);
  }

  // This client no longer knows about |window|. Unparent any windows that
  // were created by this client and parented to windows in |window|. Recursion
  // should stop at windows created by this client because the client always
  // knows about such windows, and that never changes. Only windows created by
  // other clients may be removed from the set of known windows.
  std::vector<aura::Window*> created_windows;
  RemoveWindowFromKnownWindowsRecursive(window, &created_windows);
  for (aura::Window* created_window : created_windows) {
    if (created_window != window && created_window->parent())
      created_window->parent()->RemoveChild(created_window);
  }

  if (reason == DeleteClientRootReason::kUnembed ||
      reason == DeleteClientRootReason::kDestructor) {
    // Notify the owner of the window it no longer has a client embedded in it.
    ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
    if (server_window->owning_window_tree() &&
        server_window->owning_window_tree() != this) {
      // ClientRoots always trigger creation of a ServerWindow, so
      // |server_window| must exist at this point.
      DCHECK(server_window);
      server_window->owning_window_tree()
          ->window_tree_client_->OnEmbeddedAppDisconnected(
              server_window->owning_window_tree()->TransportIdForWindow(
                  window));
    }
    if (server_window->embedding())
      server_window->embedding()->clear_embedded_tree();
    // Only reset the embedding if it's for an existing tree. To do otherwise
    // results in trying to delete this.
    if (server_window->embedding() && !server_window->embedding()->binding()) {
      server_window->SetEmbedding(nullptr);
      if (!server_window->owning_window_tree())
        server_window->Destroy();
    }
  }
}

void WindowTree::DeleteClientRootWithRoot(aura::Window* window) {
  auto iter = FindClientRootWithRoot(window);
  if (iter == client_roots_.end())
    return;

  DeleteClientRoot(iter->get(), DeleteClientRootReason::kEmbed);
}

aura::Window* WindowTree::GetWindowByClientId(const ClientWindowId& id) {
  auto iter = client_window_id_to_window_map_.find(id);
  return iter == client_window_id_to_window_map_.end() ? nullptr : iter->second;
}

bool WindowTree::IsClientCreatedWindow(aura::Window* window) {
  auto iter = known_windows_map_.find(window);
  return iter == known_windows_map_.end() ? false
                                          : iter->second.is_client_created;
}

bool WindowTree::IsClientRootWindow(aura::Window* window) {
  return window && FindClientRootWithRoot(window) != client_roots_.end();
}

ClientRoot* WindowTree::FindClientRootContaining(aura::Window* window) {
  if (!window)
    return nullptr;
  auto iter = FindClientRootWithRoot(window);
  if (iter != client_roots_.end())
    return iter->get();
  return FindClientRootContaining(window->parent());
}

WindowTree::ClientRoots::iterator WindowTree::FindClientRootWithRoot(
    aura::Window* window) {
  if (!window)
    return client_roots_.end();
  for (auto iter = client_roots_.begin(); iter != client_roots_.end(); ++iter) {
    if (iter->get()->window() == window)
      return iter;
  }
  return client_roots_.end();
}

bool WindowTree::IsWindowRootOfAnotherClient(aura::Window* window) const {
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  return server_window && server_window->embedded_window_tree() != nullptr &&
         server_window->embedded_window_tree() != this;
}

bool WindowTree::DoesAnyAncestorInterceptEvents(ServerWindow* window) {
  if (window->embedding() && window->embedding()->embedding_tree() != this &&
      window->embedding()->embedding_tree_intercepts_events()) {
    return true;
  }
  ServerWindow* parent = ServerWindow::GetMayBeNull(window->window()->parent());
  return parent && DoesAnyAncestorInterceptEvents(parent);
}

void WindowTree::OnCaptureLost(aura::Window* lost_capture) {
  DCHECK(IsWindowKnown(lost_capture));
  window_tree_client_->OnCaptureChanged(kInvalidTransportId,
                                        TransportIdForWindow(lost_capture));
}

void WindowTree::OnPerformWindowMoveDone(uint32_t change_id, bool result) {
  window_moving_ = nullptr;
  window_tree_client_->OnChangeCompleted(change_id, result);
}

void WindowTree::DoPerformDragDrop(
    uint32_t change_id,
    Id source_window_id,
    const gfx::Point& screen_location,
    const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
    const gfx::ImageSkia& drag_image,
    const gfx::Vector2d& drag_image_offset,
    uint32_t drag_operation,
    ::ui::mojom::PointerKind source) {
  if (pending_drag_source_window_id_ != source_window_id) {
    // Pending drag is canceled before DoPerformDragDrop runs.
    window_tree_client_->OnPerformDragDropCompleted(change_id, false,
                                                    mojom::kDropEffectNone);
    return;
  }

  aura::Window* source_window = GetWindowByTransportId(source_window_id);
  if (!source_window) {
    DVLOG(1) << "PerformDragDrop failed (no window)";
    OnPerformDragDropDone(change_id, mojom::kDropEffectNone);
    return;
  }
  if (!IsClientCreatedWindow(source_window)) {
    DVLOG(1) << "PerformDragDrop failed (access denied)";
    OnPerformDragDropDone(change_id, mojom::kDropEffectNone);
    return;
  }

  ui::OSExchangeData data(std::make_unique<aura::OSExchangeDataProviderMus>(
      mojo::FlatMapToMap(drag_data)));
  data.provider().SetDragImage(drag_image, drag_image_offset);

  window_service_->delegate()->RunDragLoop(
      source_window, data, screen_location, drag_operation,
      source == ::ui::mojom::PointerKind::MOUSE
          ? ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE
          : ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH,
      base::BindOnce(&WindowTree::OnPerformDragDropDone,
                     weak_factory_.GetWeakPtr(), change_id));
}

void WindowTree::OnPerformDragDropDone(uint32_t change_id, int drag_result) {
  pending_drag_source_window_id_ = kInvalidTransportId;
  window_tree_client_->OnPerformDragDropCompleted(
      change_id, drag_result != ui::DragDropTypes::DRAG_NONE, drag_result);
}

aura::Window* WindowTree::FindFirstClientCreatedWindow() {
  for (auto& pair : known_windows_map_) {
    if (pair.second.is_client_created)
      return pair.first;
  }
  return nullptr;
}

aura::Window* WindowTree::AddClientCreatedWindow(
    const ClientWindowId& id,
    bool is_top_level,
    std::unique_ptr<aura::Window> window_ptr) {
  aura::Window* window = window_ptr.get();
  ServerWindow::Create(window, this, id, is_top_level);
  AddWindowToKnownWindows(window, id, std::move(window_ptr));
  return window;
}

void WindowTree::AddWindowToKnownWindows(
    aura::Window* window,
    const ClientWindowId& id,
    std::unique_ptr<aura::Window> owned_window) {
  DCHECK(!IsWindowKnown(window));
  KnownWindow& known_window = known_windows_map_[window];
  known_window.client_window_id = id;
  known_window.is_client_created = owned_window.get() != nullptr;
  known_window.owned_window = std::move(owned_window);

  DCHECK(IsWindowKnown(window));
  client_window_id_to_window_map_[id] = window;
  if (IsClientCreatedWindow(window))
    window->AddObserver(this);
}

void WindowTree::RemoveWindowFromKnownWindows(aura::Window* window,
                                              bool delete_if_owned) {
  DCHECK(IsWindowKnown(window));

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  ClientRoot* client_root = FindClientRootContaining(window);
  if (client_root)
    client_root->UnattachChildFrameSinkIdRecursive(server_window);

  server_window->set_attached_frame_sink_id(viz::FrameSinkId());

  auto iter = known_windows_map_.find(window);
  DCHECK(iter != known_windows_map_.end());
  if (iter->second.owned_window) {
    window->RemoveObserver(this);
    if (!delete_if_owned) {
      // |window| is in the process of being deleted, release() to avoid double
      // deletion.
      iter->second.owned_window.release();
    }
    iter->second.owned_window.reset();
  }
  // Sanity check to make sure deletion didn't result in removal
  DCHECK(iter == known_windows_map_.find(window));

  // Remove from these maps after destruction. This is necessary as destruction
  // may end up expecting to find a ServerWindow.
  DCHECK(iter != known_windows_map_.end());
  client_window_id_to_window_map_.erase(iter->second.client_window_id);
  known_windows_map_.erase(iter);
}

void WindowTree::RemoveWindowFromKnownWindowsRecursive(
    aura::Window* window,
    std::vector<aura::Window*>* created_windows) {
  if (IsClientCreatedWindow(window)) {
    // Stop iterating at windows created by this client. We assume the client
    // will keep seeing any descendants.
    if (created_windows)
      created_windows->push_back(window);
    return;
  }

  if (IsWindowKnown(window)) {
    const bool delete_if_owned = true;
    RemoveWindowFromKnownWindows(window, delete_if_owned);
  }

  for (aura::Window* child : window->children())
    RemoveWindowFromKnownWindowsRecursive(child, created_windows);
}

bool WindowTree::IsValidIdForNewWindow(const ClientWindowId& id) const {
  return client_window_id_to_window_map_.count(id) == 0u &&
         base::checked_cast<ClientSpecificId>(id.client_id()) == client_id_;
}

Id WindowTree::ClientWindowIdToTransportId(
    const ClientWindowId& client_window_id) const {
  if (client_window_id.client_id() == client_id_)
    return client_window_id.sink_id();
  const Id client_id = client_window_id.client_id();
  return (client_id << 32) | client_window_id.sink_id();
}

Id WindowTree::TransportIdForWindow(aura::Window* window) const {
  DCHECK(IsWindowKnown(window));
  return ClientWindowIdToTransportId(ClientWindowIdForWindow(window));
}

ClientWindowId WindowTree::MakeClientWindowId(Id transport_window_id) const {
  if (!ClientIdFromTransportId(transport_window_id))
    return ClientWindowId(client_id_, transport_window_id);
  return ClientWindowId(ClientIdFromTransportId(transport_window_id),
                        ClientWindowIdFromTransportId(transport_window_id));
}

bool WindowTree::IsLocalSurfaceIdAssignedByClient(aura::Window* window) {
  return !IsTopLevel(window) && IsClientCreatedWindow(window);
}

std::vector<mojom::WindowDataPtr> WindowTree::WindowsToWindowDatas(
    const std::vector<aura::Window*>& windows) {
  std::vector<mojom::WindowDataPtr> array(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    array[i] = WindowToWindowData(windows[i]);
  return array;
}

mojom::WindowDataPtr WindowTree::WindowToWindowData(aura::Window* window) {
  aura::Window* parent = window->parent();
  aura::Window* transient_parent =
      aura::client::GetTransientWindowClient()->GetTransientParent(window);

  // If a window isn't known, it means it is not known to the client and should
  // not be sent over.
  if (!IsWindowKnown(parent))
    parent = nullptr;
  if (!IsWindowKnown(transient_parent))
    transient_parent = nullptr;
  mojom::WindowDataPtr window_data(mojom::WindowData::New());
  window_data->parent_id =
      parent ? TransportIdForWindow(parent) : kInvalidTransportId;
  window_data->window_id =
      window ? TransportIdForWindow(window) : kInvalidTransportId;
  window_data->transient_parent_id =
      transient_parent ? TransportIdForWindow(transient_parent)
                       : kInvalidTransportId;
  window_data->bounds =
      IsTopLevel(window) ? window->GetBoundsInScreen() : window->bounds();
  window_data->properties =
      window_service_->property_converter()->GetTransportProperties(window);
  window_data->visible = window->TargetVisibility();
  return window_data;
}

mojom::WindowTreeClientPtr
WindowTree::GetAndRemoveScheduledEmbedWindowTreeClient(
    const base::UnguessableToken& token,
    std::set<WindowTree*>* visited_trees) {
  if (visited_trees->count(this))
    return nullptr;

  auto iter = scheduled_embeds_.find(token);
  if (iter != scheduled_embeds_.end()) {
    mojom::WindowTreeClientPtr client = std::move(iter->second);
    scheduled_embeds_.erase(iter);
    return client;
  }

  visited_trees->insert(this);
  for (auto& client_root : client_roots_) {
    ServerWindow* root_window =
        ServerWindow::GetMayBeNull(client_root->window());
    DCHECK(root_window);  // There should always be a ServerWindow for a root.
    WindowTree* owning_tree = root_window->owning_window_tree();
    if (owning_tree) {
      auto result = owning_tree->GetAndRemoveScheduledEmbedWindowTreeClient(
          token, visited_trees);
      if (result)
        return result;
    }
  }
  return nullptr;
}

void WindowTree::SendTopmostWindows(
    const std::vector<aura::Window*>& topmosts) {
  DCHECK_NE(connection_type_, ConnectionType::kEmbedding);
  std::vector<Id> topmost_ids;
  for (auto* window : topmosts) {
    topmost_ids.push_back(IsWindowKnown(window) ? TransportIdForWindow(window)
                                                : kInvalidTransportId);
  }
  window_tree_client_->OnTopmostWindowChanged(topmost_ids);
}

void WindowTree::SendOcclusionState(aura::Window* window) {
  DCHECK(IsWindowKnown(window));

  window_tree_client_->OnOcclusionStateChanged(
      TransportIdForWindow(window),
      aura::WindowOcclusionStateToMojom(window->occlusion_state()));
}

bool WindowTree::NewWindowImpl(
    const ClientWindowId& client_window_id,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  DVLOG(3) << "new window client=" << client_id_
           << " window_id=" << client_window_id.ToString();
  if (!IsValidIdForNewWindow(client_window_id)) {
    DVLOG(1) << "NewWindow failed (id is not valid for client)";
    return false;
  }
  const bool is_top_level = false;
  // WindowDelegateImpl deletes itself when |window| is destroyed.
  WindowDelegateImpl* window_delegate = new WindowDelegateImpl();
  std::unique_ptr<aura::Window> window_ptr = std::make_unique<aura::Window>(
      window_delegate, aura::client::WINDOW_TYPE_UNKNOWN,
      window_service_->env());
  window_delegate->set_window(window_ptr.get());
  aura::Window* window = AddClientCreatedWindow(client_window_id, is_top_level,
                                                std::move(window_ptr));

  SetWindowType(window, aura::GetWindowTypeFromProperties(properties));
  for (auto& pair : properties) {
    window_service_->property_converter()->SetPropertyFromTransportValue(
        window, pair.first, &pair.second);
  }
  window->Init(ui::LAYER_NOT_DRAWN);
  // Windows created by the client should only be destroyed by the client.
  window->set_owned_by_parent(false);
  return true;
}

bool WindowTree::DeleteWindowImpl(const ClientWindowId& window_id) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "deleting window client=" << client_id_
           << " client window_id=" << window_id.ToString();
  if (!window) {
    DVLOG(1) << "DeleteWindow failed (no window)";
    return false;
  }

  const bool is_client_created_window = IsClientCreatedWindow(window);
  auto iter = FindClientRootWithRoot(window);
  if (iter != client_roots_.end()) {
    DeleteClientRoot(iter->get(), DeleteClientRootReason::kUnembed);
    if (!is_client_created_window)
      return true;
    // If client created, fall through to delete window.
  } else if (!is_client_created_window) {
    DVLOG(1) << "DeleteWindow failed (client did not create window)";
    return false;
  }

  const bool delete_if_owned = true;
  RemoveWindowFromKnownWindows(window, delete_if_owned);
  return true;
}

bool WindowTree::SetCaptureImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "SetCapture window_id=" << window_id;
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "SetCapture failed (no window)";
    return false;
  }

  if ((!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) ||
      !window->IsVisible() || !window->GetRootWindow()) {
    DVLOG(1) << "SetCapture failed (access denied or invalid window)";
    return false;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);

  if (DoesAnyAncestorInterceptEvents(server_window)) {
    // If an ancestor is intercepting events, than the descendants are not
    // allowed to set capture. This is primarily to prevent renderers from
    // setting capture.
    DVLOG(1) << "SetCapture failed (ancestor intercepts events)";
    return false;
  }

  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  DCHECK(capture_controller);

  if (capture_controller->GetCaptureWindow() == window) {
    if (server_window->capture_owner() != this) {
      // The capture window didn't change, but the client that owns capture
      // changed (see |ServerWindow::capture_owner_| for details on this).
      // Notify the current owner that it lost capture.
      if (server_window->capture_owner())
        server_window->capture_owner()->OnCaptureLost(window);
      server_window->SetCaptureOwner(this);
    }
    return true;
  }

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kCapture);
  server_window->SetCaptureOwner(this);
  capture_controller->SetCapture(window);
  return capture_controller->GetCaptureWindow() == window;
}

bool WindowTree::ReleaseCaptureImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "ReleaseCapture window_id=" << window_id;
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "ReleaseCapture failed (no window)";
    return false;
  }

  if (!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    DVLOG(1) << "ReleaseCapture failed (access denied)";
    return false;
  }

  wm::CaptureController* capture_controller = wm::CaptureController::Get();
  DCHECK(capture_controller);

  if (!capture_controller->GetCaptureWindow())
    return true;  // Capture window is already null.

  if (capture_controller->GetCaptureWindow() != window) {
    DVLOG(1) << "ReleaseCapture failed (supplied window does not have capture)";
    return false;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  if (server_window->capture_owner() &&
      server_window->capture_owner() != this) {
    // This client is trying to release capture, but it doesn't own capture.
    DVLOG(1) << "ReleaseCapture failed (client did not request capture)";
    return false;
  }
  server_window->SetCaptureOwner(nullptr);

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kCapture);
  capture_controller->ReleaseCapture(window);
  return capture_controller->GetCaptureWindow() != window;
}

bool WindowTree::AddWindowImpl(const ClientWindowId& parent_id,
                               const ClientWindowId& child_id) {
  aura::Window* parent = GetWindowByClientId(parent_id);
  aura::Window* child = GetWindowByClientId(child_id);
  DVLOG(3) << "add window client=" << client_id_
           << " client parent window_id=" << parent_id.ToString()
           << " client child window_id=" << child_id.ToString();
  if (!parent) {
    DVLOG(1) << "AddWindow failed (no parent)";
    return false;
  }
  if (!child) {
    DVLOG(1) << "AddWindow failed (no child)";
    return false;
  }
  if (child->parent() == parent) {
    DVLOG(1) << "AddWindow failed (already has parent)";
    return false;
  }
  if (child->Contains(parent)) {
    DVLOG(1) << "AddWindow failed (child contains parent)";
    return false;
  }
  if (IsClientCreatedWindow(child) && !IsTopLevel(child) &&
      (IsClientRootWindow(parent) || (IsClientCreatedWindow(parent) &&
                                      !IsWindowRootOfAnotherClient(parent)))) {
    parent->AddChild(child);
    return true;
  }
  DVLOG(1) << "AddWindow failed (access denied)";
  return false;
}

bool WindowTree::RemoveWindowFromParentImpl(
    const ClientWindowId& client_window_id) {
  aura::Window* window = GetWindowByClientId(client_window_id);
  DVLOG(3) << "removing window from parent client=" << client_id_
           << " client window_id=" << client_window_id;
  if (!window) {
    DVLOG(1) << "RemoveWindowFromParent failed (invalid window id="
             << client_window_id.ToString() << ")";
    return false;
  }
  if (!window->parent()) {
    DVLOG(1) << "RemoveWindowFromParent failed (no parent id="
             << client_window_id.ToString() << ")";
    return false;
  }
  if (IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    window->parent()->RemoveChild(window);
    return true;
  }
  DVLOG(1) << "RemoveWindowFromParent failed (access policy disallowed id="
           << client_window_id.ToString() << ")";
  return false;
}

bool WindowTree::AddTransientWindowImpl(const ClientWindowId& parent_id,
                                        const ClientWindowId& transient_id) {
  DVLOG(3) << "adding transient window client=" << client_id_
           << " parent_id=" << parent_id << " transient_id=" << transient_id;
  aura::Window* parent = GetWindowByClientId(parent_id);
  aura::Window* transient = GetWindowByClientId(transient_id);
  if (!parent || !transient) {
    DVLOG(1) << "AddTransientWindow failed (invalid window parent_id="
             << parent_id << " transient_id=" << transient_id << ")";
    return false;
  }

  if (parent->Contains(transient)) {
    DVLOG(1) << "AddTransientWindow failed (parent contains transient"
             << " parent_id=" << parent_id << " transient_id=" << transient_id
             << ")";
    return false;
  }

  if (!IsClientCreatedWindow(parent) || !IsClientCreatedWindow(transient)) {
    DVLOG(1) << "SetModalType failed (access policy disallowed parent_id="
             << parent_id << " transient_id=" << transient_id << ")";
    return false;
  }

  ::wm::AddTransientChild(parent, transient);
  return true;
}

bool WindowTree::RemoveTransientWindowFromParentImpl(
    const ClientWindowId& transient_id) {
  DVLOG(3) << "removing transient window from parent client=" << client_id_
           << " transient_id=" << transient_id;
  aura::Window* transient = GetWindowByClientId(transient_id);
  aura::Window* parent = ::wm::GetTransientParent(transient);
  if (!parent || !transient) {
    DVLOG(1) << "AddTransientWindow failed (invalid window or no transient"
             << " parent transient_id=" << transient_id << ")";
    return false;
  }

  if (!IsClientCreatedWindow(parent) || !IsClientCreatedWindow(transient)) {
    DVLOG(1) << "SetModalType failed (access policy disallowed transient_id="
             << transient_id << ")";
    return false;
  }

  ::wm::RemoveTransientChild(parent, transient);
  return true;
}

bool WindowTree::SetModalTypeImpl(const ClientWindowId& client_window_id,
                                  ui::ModalType type) {
  DVLOG(3) << "setting window modal type client=" << client_id_
           << " client_window_id=" << client_window_id << " type=" << type;
  aura::Window* window = GetWindowByClientId(client_window_id);
  if (!window) {
    DVLOG(1) << "SetModalType failed (invalid window id="
             << client_window_id.ToString() << ")";
    return false;
  }

  if (!IsClientRootWindow(window) && type == ui::MODAL_TYPE_SYSTEM) {
    DVLOG(1) << "SetModalType failed (not allowed for embedded clients)";
    return false;
  }

  if (type == ui::MODAL_TYPE_SYSTEM &&
      window->type() != aura::client::WINDOW_TYPE_NORMAL &&
      window->type() != aura::client::WINDOW_TYPE_POPUP) {
    DVLOG(1) << "Window type cannot be made system modal: " << window->type();
    return false;
  }

  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetModalType failed (access policy disallowed id="
             << client_window_id.ToString() << ")";
    return false;
  }

  window_service_->delegate()->SetModalType(window, type);
  return true;
}

bool WindowTree::SetWindowVisibilityImpl(const ClientWindowId& window_id,
                                         bool visible) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetWindowVisibility client=" << client_id_
           << " client window_id=" << window_id.ToString();
  if (!window) {
    DVLOG(1) << "SetWindowVisibility failed (no window)";
    return false;
  }
  if (IsClientCreatedWindow(window) ||
      (IsClientRootWindow(window) && can_change_root_window_visibility_)) {
    if (window->TargetVisibility() == visible)
      return true;
    ClientChange change(property_change_tracker_.get(), window,
                        ClientChangeType::kVisibility);
    if (visible)
      window->Show();
    else
      window->Hide();
    return true;
  }
  DVLOG(1) << "SetWindowVisibility failed (access policy denied change)";
  return false;
}

bool WindowTree::SetWindowPropertyImpl(
    const ClientWindowId& window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& value) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetWindowProperty client=" << client_id_
           << " client window_id=" << window_id.ToString();
  if (!window) {
    DVLOG(1) << "SetWindowProperty failed (no window)";
    return false;
  }
  aura::PropertyConverter* property_converter =
      window_service_->property_converter();
  if (!property_converter->IsTransportNameRegistered(name)) {
    NOTREACHED() << "Attempting to set an unregistered property; this is not "
                    "implemented. property name="
                 << name;
    return false;
  }
  if (!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    DVLOG(1) << "SetWindowProperty failed (access policy denied change)";
    return false;
  }

  ClientChange change(
      property_change_tracker_.get(), window, ClientChangeType::kProperty,
      property_converter->GetPropertyKeyFromTransportName(name));

  // Special handle the property whose value is a pointer to aura::Window since
  // property converter can't convert the transported value.
  const aura::WindowProperty<aura::Window*>* property =
      property_converter->GetWindowPtrProperty(name);
  if (property) {
    aura::Window* prop_window = nullptr;
    if (value.has_value())
      prop_window = GetWindowByTransportId(mojo::ConvertTo<Id>(value.value()));
    window->SetProperty(property, prop_window);
    return true;
  }

  std::unique_ptr<std::vector<uint8_t>> data;
  if (value.has_value())
    data = std::make_unique<std::vector<uint8_t>>(value.value());
  property_converter->SetPropertyFromTransportValue(window, name, data.get());
  return true;
}

bool WindowTree::EmbedImpl(const ClientWindowId& window_id,
                           mojom::WindowTreeClientPtr window_tree_client_ptr,
                           mojom::WindowTreeClient* window_tree_client,
                           uint32_t flags) {
  DVLOG(3) << "Embed window_id=" << window_id;
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "Embed failed (no window)";
    return false;
  }
  if (!IsClientCreatedWindow(window) || IsTopLevel(window)) {
    DVLOG(1) << "Embed failed (access denied)";
    return false;
  }

  const bool owner_intercept_events =
      (connection_type_ != ConnectionType::kEmbedding &&
       (flags & mojom::kEmbedFlagEmbedderInterceptsEvents) != 0);
  std::unique_ptr<Embedding> embedding =
      std::make_unique<Embedding>(this, window, owner_intercept_events);
  embedding->Init(window_service_, std::move(window_tree_client_ptr),
                  window_tree_client,
                  base::BindOnce(&WindowTree::OnEmbeddedClientConnectionLost,
                                 base::Unretained(this), embedding.get()));
  if (flags & mojom::kEmbedFlagEmbedderControlsVisibility)
    embedding->embedded_tree()->can_change_root_window_visibility_ = false;
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  server_window->SetEmbedding(std::move(embedding));
  window_tree_client_->OnFrameSinkIdAllocated(
      ClientWindowIdToTransportId(window_id), server_window->frame_sink_id());
  return true;
}

bool WindowTree::SetWindowOpacityImpl(const ClientWindowId& window_id,
                                      float opacity) {
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetWindowOpacity client=" << client_id_
           << " client window_id=" << window_id.ToString();
  if (IsClientCreatedWindow(window) ||
      (IsClientRootWindow(window) && can_change_root_window_visibility_)) {
    if (window->layer()->opacity() == opacity)
      return true;
    window->layer()->SetOpacity(opacity);
    return true;
  }
  DVLOG(1) << "SetWindowOpacity failed (invalid window or access denied)";
  return false;
}

bool WindowTree::SetWindowBoundsImpl(
    const ClientWindowId& window_id,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  aura::Window* window = GetWindowByClientId(window_id);

  DVLOG(3) << "SetWindowBounds window_id=" << window_id
           << " bounds=" << bounds.ToString() << " local_surface_id="
           << (local_surface_id ? local_surface_id->ToString() : "null");

  if (!window) {
    DVLOG(1) << "SetWindowBounds failed (invalid window id)";
    return false;
  }

  // Only the owner of the window can change the bounds.
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetWindowBounds failed (access denied)";
    return false;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  const gfx::Rect original_bounds =
      IsTopLevel(window) ? window->GetBoundsInScreen() : window->bounds();
  const bool local_surface_id_changed =
      server_window->local_surface_id() != local_surface_id;

  if (original_bounds == bounds && !local_surface_id_changed)
    return true;

  ClientChange change(property_change_tracker_.get(), window,
                      ClientChangeType::kBounds);

  if (IsLocalSurfaceIdAssignedByClient(window))
    server_window->set_local_surface_id(local_surface_id);

  if (IsTopLevel(window)) {
    display::Display dst_display =
        display::Screen::GetScreen()->GetDisplayMatching(bounds);
    window->SetBoundsInScreen(bounds, dst_display);
  } else {
    window->SetBounds(bounds);
  }
  if (!change.window())
    return true;  // Return value doesn't matter if window destroyed.

  if (IsClientRootWindow(window)) {
    // ClientRoot handles notification in this case. Note that this
    // unconditionally returns false, because the LocalSurfaceId changes with
    // the bounds. Returning false ensures the client applies the LocalSurfaceId
    // assigned by ClientRoot and sent to the client in
    // ClientRoot::OnWindowBoundsChanged().
    return false;
  }

  if (window->bounds() == original_bounds) {
    if (local_surface_id_changed) {
      // If the bounds didn't change, but the LocalSurfaceId did, then the
      // LocalSurfaceId needs to be propagated to any embeddings.
      if (server_window->HasEmbedding() &&
          server_window->embedding()->embedding_tree() == this) {
        WindowTree* embedded_tree = server_window->embedding()->embedded_tree();
        ClientRoot* embedded_client_root =
            embedded_tree->GetClientRootForWindow(window);
        DCHECK(embedded_client_root);
        embedded_client_root->OnLocalSurfaceIdChanged();
      }
    }
    return (bounds == original_bounds);
  }

  if (window->bounds() == bounds &&
      server_window->local_surface_id() == local_surface_id) {
    return true;
  }

  // The window's bounds changed, but not to the value the client requested.
  // Tell the client the new value, and return false, which triggers the client
  // to use the value supplied to OnWindowBoundsChanged().
  window_tree_client_->OnWindowBoundsChanged(TransportIdForWindow(window),
                                             original_bounds, window->bounds(),
                                             local_surface_id);
  return false;
}

bool WindowTree::ReorderWindowImpl(const ClientWindowId& window_id,
                                   const ClientWindowId& relative_window_id,
                                   mojom::OrderDirection direction) {
  DVLOG(3) << "ReorderWindow window_id=" << window_id
           << " relative_window_id=" << relative_window_id;
  aura::Window* window = GetWindowByClientId(window_id);
  aura::Window* relative_window = GetWindowByClientId(relative_window_id);
  // Only allow reordering of windows the client created, and windows that are
  // siblings.
  if (!IsClientCreatedWindow(window) ||
      !IsClientCreatedWindow(relative_window) ||
      window->parent() != relative_window->parent() || !window->parent() ||
      !IsClientCreatedWindow(window->parent())) {
    DVLOG(1) << "ReorderWindow failed (invalid windows)";
    return false;
  }
  if (direction == mojom::OrderDirection::ABOVE)
    window->parent()->StackChildAbove(window, relative_window);
  else
    window->parent()->StackChildBelow(window, relative_window);
  return true;
}

std::vector<aura::Window*> WindowTree::GetWindowTreeImpl(
    const ClientWindowId& window_id) {
  aura::Window* window = GetWindowByClientId(window_id);
  std::vector<aura::Window*> results;
  GetWindowTreeRecursive(window, &results);
  return results;
}

bool WindowTree::SetFocusImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "SetFocus window_id=" << window_id;
  // FocusHandler deals with a null window.
  return focus_handler_.SetFocus(GetWindowByClientId(window_id));
}

bool WindowTree::SetCursorImpl(const ClientWindowId& window_id,
                               ui::CursorData cursor) {
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window) {
    DVLOG(1) << "SetCursor failed (no window)";
    return false;
  }
  if (!IsClientCreatedWindow(window) && !IsClientRootWindow(window)) {
    DVLOG(1) << "SetCursor failed (access denied)";
    return false;
  }

  auto* server_window = ServerWindow::GetMayBeNull(window);

  ui::Cursor old_cursor_type = cursor.ToNativeCursor();

  // Ask our delegate to set the cursor. This will save the cursor for toplevels
  // and also update the active cursor if appropriate (i.e. if |window| is the
  // last to have set the cursor/is currently hovered).
  if (!window_service_->delegate()->StoreAndSetCursor(window,
                                                      old_cursor_type)) {
    // Store the cursor on ServerWindow. This will later be accessed by the
    // WindowDelegate for non-toplevels, i.e. WindowDelegateImpl.
    server_window->StoreCursor(old_cursor_type);
  }

  return true;
}

bool WindowTree::StackAboveImpl(const ClientWindowId& above_window_id,
                                const ClientWindowId& below_window_id) {
  DVLOG(3) << "StackAbove above_window_id=" << above_window_id
           << " below_window_id=" << below_window_id;
  aura::Window* above_window = GetWindowByClientId(above_window_id);
  aura::Window* below_window = GetWindowByClientId(below_window_id);
  // This function only applies to top-levels.
  if (!IsClientCreatedWindow(above_window) ||
      !IsClientCreatedWindow(below_window) || !IsTopLevel(above_window) ||
      !IsTopLevel(below_window) || !above_window->parent() ||
      above_window->parent() != below_window->parent()) {
    DVLOG(1) << "StackAbove failed (invalid windows)";
    return false;
  }
  above_window->parent()->StackChildAbove(above_window, below_window);
  return true;
}

bool WindowTree::StackAtTopImpl(const ClientWindowId& window_id) {
  DVLOG(3) << "StackAtTop window_id=" << window_id;

  aura::Window* window = GetWindowByClientId(window_id);
  if (!window || !IsTopLevel(window)) {
    DVLOG(1) << "StackAtTop failed (invalid id, or invalid window)";
    return false;
  }

  if (window->parent())
    window->parent()->StackChildAtTop(window);
  return true;
}

void WindowTree::GetWindowTreeRecursive(aura::Window* window,
                                        std::vector<aura::Window*>* windows) {
  if (!IsWindowKnown(window))
    return;

  windows->push_back(window);
  for (aura::Window* child : window->children())
    GetWindowTreeRecursive(child, windows);
}

void WindowTree::OnEmbeddedClientConnectionLost(Embedding* embedding) {
  ServerWindow::GetMayBeNull(embedding->window())->SetEmbedding(nullptr);
}

void WindowTree::OnWindowHierarchyChanging(
    const HierarchyChangeParams& params) {
  if (params.target != params.receiver || !IsClientCreatedWindow(params.target))
    return;

  ServerWindow* server_window = ServerWindow::GetMayBeNull(params.target);
  DCHECK(server_window);  // non-null because of IsClientCreatedWindow() check.
  ClientRoot* old_root = FindClientRootContaining(params.old_parent);
  ClientRoot* new_root = FindClientRootContaining(params.new_parent);
  if (old_root == new_root)
    return;

  if (old_root)
    old_root->UnattachChildFrameSinkIdRecursive(server_window);
  if (new_root)
    new_root->AttachChildFrameSinkIdRecursive(server_window);
}

void WindowTree::OnWindowDestroyed(aura::Window* window) {
  DCHECK(IsWindowKnown(window));

  // WARNING: this function is not necessarily called. In particular it isn't
  // called when the client requests the window to be deleted, or from the
  // destructor.

  auto iter = FindClientRootWithRoot(window);
  if (iter != client_roots_.end())
    DeleteClientRoot(iter->get(), WindowTree::DeleteClientRootReason::kDeleted);

  DCHECK(IsWindowKnown(window));
  window_tree_client_->OnWindowDeleted(TransportIdForWindow(window));

  const bool delete_if_owned = false;
  RemoveWindowFromKnownWindows(window, delete_if_owned);
}

void WindowTree::OnWindowVisibilityChanging(aura::Window* window,
                                            bool visible) {
  if (property_change_tracker_->IsProcessingChangeForWindow(
          window, ClientChangeType::kVisibility)) {
    return;
  }
  window_tree_client_->OnWindowVisibilityChanged(TransportIdForWindow(window),
                                                 visible);
}

void WindowTree::OnCaptureChanged(aura::Window* lost_capture,
                                  aura::Window* gained_capture) {
  if (property_change_tracker_->IsProcessingChangeForWindow(
          lost_capture, ClientChangeType::kCapture) ||
      property_change_tracker_->IsProcessingChangeForWindow(
          gained_capture, ClientChangeType::kCapture)) {
    // The client initiated the change, don't notify the client.
    return;
  }

  // Assume the environment the WindowService is running in is not requesting
  // capture on windows created by clients. With this assumption, the only time
  // the client needs to be notified is if the client had set capture on one of
  // its windows, and capture changed. This might happen if the window is no
  // longer valid for capture, or the local environment requests capture on
  // another window.
  if (lost_capture && (IsClientCreatedWindow(lost_capture) ||
                       IsClientRootWindow(lost_capture))) {
    ServerWindow* server_window = ServerWindow::GetMayBeNull(lost_capture);
    if (server_window->capture_owner() == this) {
      // One of the windows known to this client had capture. Notify the client
      // of the change. If the client does not know about the window that gained
      // capture, an invalid window id is used.
      server_window->SetCaptureOwner(nullptr);
      const Id gained_capture_id = gained_capture &&
                                           IsWindowKnown(gained_capture) &&
                                           !IsClientRootWindow(gained_capture)
                                       ? TransportIdForWindow(gained_capture)
                                       : kInvalidTransportId;
      window_tree_client_->OnCaptureChanged(gained_capture_id,
                                            TransportIdForWindow(lost_capture));
    }
  }
}

void WindowTree::NewWindow(
    uint32_t change_id,
    Id transport_window_id,
    const base::Optional<base::flat_map<std::string, std::vector<uint8_t>>>&
        transport_properties) {
  std::map<std::string, std::vector<uint8_t>> properties;
  if (transport_properties.has_value())
    properties = mojo::FlatMapToMap(transport_properties.value());
  window_tree_client_->OnChangeCompleted(
      change_id,
      NewWindowImpl(MakeClientWindowId(transport_window_id), properties));
}

void WindowTree::NewTopLevelWindow(
    uint32_t change_id,
    Id transport_window_id,
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  const ClientWindowId client_window_id =
      MakeClientWindowId(transport_window_id);
  DVLOG(3) << "NewTopLevelWindow client_window_id="
           << client_window_id.ToString();
  if (!IsValidIdForNewWindow(client_window_id)) {
    DVLOG(1) << "NewTopLevelWindow failed (invalid window id)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }
  if (connection_type_ == ConnectionType::kEmbedding) {
    // This is done to disallow clients such as renderers from creating
    // top-level windows.
    DVLOG(1) << "NewTopLevelWindow failed (access denied)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }
  std::unique_ptr<aura::Window> top_level_ptr =
      window_service_->delegate()->NewTopLevel(
          window_service_->property_converter(), properties);
  if (!top_level_ptr) {
    DVLOG(1) << "NewTopLevelWindow failed (delegate window creation failed)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }
  top_level_ptr->set_owned_by_parent(false);
  const bool is_top_level = true;
  aura::Window* top_level = AddClientCreatedWindow(
      client_window_id, is_top_level, std::move(top_level_ptr));
  ServerWindow* top_level_server_window = ServerWindow::GetMayBeNull(top_level);
  top_level_server_window->set_frame_sink_id(client_window_id);
  const int64_t display_id =
      display::Screen::GetScreen()->GetDisplayNearestWindow(top_level).id();
  // This passes null for the mojom::WindowTreePtr because the client has
  // already been given the mojom::WindowTreePtr that is backed by this
  // WindowTree.
  CreateClientRoot(top_level, is_top_level)->RegisterVizEmbeddingSupport();
  window_tree_client_->OnTopLevelCreated(
      change_id, WindowToWindowData(top_level), display_id,
      top_level->IsVisible(), top_level_server_window->local_surface_id());
}

void WindowTree::DeleteWindow(uint32_t change_id, Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, DeleteWindowImpl(MakeClientWindowId(transport_window_id)));
}

void WindowTree::SetCapture(uint32_t change_id, Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetCaptureImpl(MakeClientWindowId(transport_window_id)));
}

void WindowTree::ReleaseCapture(uint32_t change_id, Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, ReleaseCaptureImpl(MakeClientWindowId(transport_window_id)));
}

void WindowTree::ObserveEventTypes(
    const std::vector<ui::mojom::EventType>& types) {
  if (types.empty()) {
    event_observer_helper_.reset();
  } else {
    if (!event_observer_helper_)
      event_observer_helper_ = std::make_unique<EventObserverHelper>(this);
    std::set<ui::EventType> event_types;
    for (auto type : types)
      event_types.insert(mojo::ConvertTo<ui::EventType>(type));
    event_observer_helper_->set_types(event_types);
  }
}

void WindowTree::SetWindowBounds(
    uint32_t change_id,
    Id window_id,
    const gfx::Rect& bounds,
    const base::Optional<viz::LocalSurfaceId>& local_surface_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetWindowBoundsImpl(MakeClientWindowId(window_id), bounds,
                                     local_surface_id));
}

void WindowTree::SetWindowTransform(uint32_t change_id,
                                    Id window_id,
                                    const gfx::Transform& transform) {
  // NOTE: Tests may time out if they trigger this NOTIMPLEMENTED because
  // the change is not ack'd. The code under test may need to change to
  // avoid triggering window transforms outside the window manager.
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowTree::SetClientArea(
    Id transport_window_id,
    const gfx::Insets& insets,
    const base::Optional<std::vector<gfx::Rect>>& additional_client_areas) {
  const ClientWindowId window_id = MakeClientWindowId(transport_window_id);
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetClientArea client window_id=" << window_id.ToString()
           << " insets=" << insets.ToString();
  if (!window) {
    DVLOG(1) << "SetClientArea failed (invalid window id)";
    return;
  }
  if (!IsClientRootWindow(window) || !IsTopLevel(window)) {
    DVLOG(1) << "SetClientArea failed (access denied)";
    return;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  DCHECK(server_window);  // Must exist because of preceding conditionals.
  server_window->SetClientArea(
      insets, additional_client_areas.value_or(std::vector<gfx::Rect>()));
}

void WindowTree::SetHitTestInsets(Id transport_window_id,
                                  const gfx::Insets& mouse,
                                  const gfx::Insets& touch) {
  const ClientWindowId window_id = MakeClientWindowId(transport_window_id);
  aura::Window* window = GetWindowByClientId(window_id);
  DVLOG(3) << "SetHitTestInsets client window_id=" << window_id.ToString()
           << " mouse=" << mouse.ToString() << " touch=" << touch.ToString();
  if (!window) {
    DVLOG(1) << "SetHitTestInsets failed (invalid window id)";
    return;
  }
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetHitTestInsets failed (access denied)";
    return;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  DCHECK(server_window);  // Must exist because of preceding conditionals.
  server_window->SetHitTestInsets(MakeInsetsPositive(mouse),
                                  MakeInsetsPositive(touch));
}

void WindowTree::AttachFrameSinkId(Id transport_window_id,
                                   const viz::FrameSinkId& f) {
  if (!f.is_valid()) {
    DVLOG(3) << "AttachFrameSinkId failed (invalid frame sink)";
    return;
  }
  const ClientWindowId window_id = MakeClientWindowId(transport_window_id);
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window || !IsClientCreatedWindow(window)) {
    DVLOG(3) << "AttachFrameSinkId failed (invalid window id)";
    return;
  }
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  DCHECK(server_window);  // Must exist because of preceding conditionals.
  if (server_window->attached_frame_sink_id() == f)
    return;
  if (f.is_valid() && server_window->attached_frame_sink_id().is_valid()) {
    DVLOG(3) << "AttachFrameSinkId failed (window already has frame sink)";
    return;
  }
  server_window->set_attached_frame_sink_id(f);
  ClientRoot* client_root = FindClientRootContaining(window);
  if (client_root)
    client_root->AttachChildFrameSinkId(server_window);
}

void WindowTree::UnattachFrameSinkId(Id transport_window_id) {
  const ClientWindowId window_id = MakeClientWindowId(transport_window_id);
  aura::Window* window = GetWindowByClientId(window_id);
  if (!window || !IsClientCreatedWindow(window)) {
    DVLOG(3) << "UnattachFrameSinkId failed (invalid window id)";
    return;
  }
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  DCHECK(server_window);  // Must exist because of preceding conditionals.
  if (!server_window->attached_frame_sink_id().is_valid()) {
    DVLOG(3) << "UnattachFrameSinkId failed (frame sink already cleared)";
    return;
  }

  ClientRoot* client_root = FindClientRootContaining(window);
  if (client_root)
    client_root->UnattachChildFrameSinkId(server_window);
  server_window->set_attached_frame_sink_id(viz::FrameSinkId());
}

void WindowTree::SetCanAcceptDrops(Id window_id, bool accepts_drops) {
  aura::Window* window = GetWindowByTransportId(window_id);
  if (!window) {
    DVLOG(1) << "SetCanAcceptDrops failed (no window)";
    return;
  }
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetCanAcceptDrops failed (access denied)";
    return;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  DCHECK(server_window);  // Must exist because of preceding conditionals.
  if (accepts_drops && !server_window->HasDragDropDelegate()) {
    auto drag_drop_delegate = std::make_unique<DragDropDelegate>(
        window_tree_client_, window, window_id);
    aura::client::SetDragDropDelegate(window, drag_drop_delegate.get());
    server_window->SetDragDropDelegate(std::move(drag_drop_delegate));
  } else if (!accepts_drops && server_window->HasDragDropDelegate()) {
    aura::client::SetDragDropDelegate(window, nullptr);
    server_window->SetDragDropDelegate(nullptr);
  }
}

void WindowTree::SetWindowVisibility(uint32_t change_id,
                                     Id transport_window_id,
                                     bool visible) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetWindowVisibilityImpl(
                     MakeClientWindowId(transport_window_id), visible));
}

void WindowTree::SetWindowProperty(
    uint32_t change_id,
    Id window_id,
    const std::string& name,
    const base::Optional<std::vector<uint8_t>>& value) {
  window_tree_client_->OnChangeCompleted(
      change_id,
      SetWindowPropertyImpl(MakeClientWindowId(window_id), name, value));
}

void WindowTree::SetWindowOpacity(uint32_t change_id,
                                  Id transport_window_id,
                                  float opacity) {
  window_tree_client_->OnChangeCompleted(
      change_id,
      SetWindowOpacityImpl(MakeClientWindowId(transport_window_id), opacity));
}

void WindowTree::AttachCompositorFrameSink(
    Id transport_window_id,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink,
    viz::mojom::CompositorFrameSinkClientPtr client) {
  DVLOG(3) << "AttachCompositorFrameSink id="
           << MakeClientWindowId(transport_window_id).ToString();
  aura::Window* window = GetWindowByTransportId(transport_window_id);
  if (!window) {
    DVLOG(1) << "AttachCompositorFrameSink failed (invalid window id)";
    return;
  }
  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  // If this isn't called on the root, then only allow it if there is not
  // another client embedded in the window.
  const bool allow = IsClientRootWindow(window) ||
                     (IsClientCreatedWindow(window) &&
                      server_window->embedded_window_tree() == nullptr);
  if (!allow) {
    DVLOG(1) << "AttachCompositorFrameSink failed (policy disallowed)";
    return;
  }

  server_window->AttachCompositorFrameSink(std::move(compositor_frame_sink),
                                           std::move(client));
}

void WindowTree::AddWindow(uint32_t change_id, Id parent_id, Id child_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, AddWindowImpl(MakeClientWindowId(parent_id),
                               MakeClientWindowId(child_id)));
}

void WindowTree::RemoveWindowFromParent(uint32_t change_id, Id window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, RemoveWindowFromParentImpl(MakeClientWindowId(window_id)));
}

void WindowTree::AddTransientWindow(uint32_t change_id,
                                    Id window_id,
                                    Id transient_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id,
      AddTransientWindowImpl(MakeClientWindowId(window_id),
                             MakeClientWindowId(transient_window_id)));
}

void WindowTree::RemoveTransientWindowFromParent(uint32_t change_id,
                                                 Id transient_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, RemoveTransientWindowFromParentImpl(
                     MakeClientWindowId(transient_window_id)));
}

void WindowTree::SetModalType(uint32_t change_id,
                              Id window_id,
                              ui::ModalType type) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetModalTypeImpl(MakeClientWindowId(window_id), type));
}

void WindowTree::ReorderWindow(uint32_t change_id,
                               Id transport_window_id,
                               Id transport_relative_window_id,
                               mojom::OrderDirection direction) {
  const bool result = ReorderWindowImpl(
      MakeClientWindowId(transport_window_id),
      MakeClientWindowId(transport_relative_window_id), direction);
  window_tree_client_->OnChangeCompleted(change_id, result);
}

void WindowTree::GetWindowTree(Id window_id, GetWindowTreeCallback callback) {
  std::vector<aura::Window*> windows =
      GetWindowTreeImpl(MakeClientWindowId(window_id));
  std::move(callback).Run(WindowsToWindowDatas(windows));
}

void WindowTree::Embed(Id transport_window_id,
                       mojom::WindowTreeClientPtr client_ptr,
                       uint32_t embed_flags,
                       EmbedCallback callback) {
  mojom::WindowTreeClient* client = client_ptr.get();
  std::move(callback).Run(EmbedImpl(MakeClientWindowId(transport_window_id),
                                    std::move(client_ptr), client,
                                    embed_flags));
}

void WindowTree::ScheduleEmbed(mojom::WindowTreeClientPtr client,
                               ScheduleEmbedCallback callback) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  DCHECK(!scheduled_embeds_.count(token));
  scheduled_embeds_[token] = std::move(client);
  std::move(callback).Run(token);
}

void WindowTree::ScheduleEmbedForExistingClient(
    uint32_t window_id,
    ScheduleEmbedForExistingClientCallback callback) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  DCHECK(!scheduled_embeds_for_existing_client_.count(token));
  scheduled_embeds_for_existing_client_[token] = window_id;
  std::move(callback).Run(token);
}

void WindowTree::EmbedUsingToken(Id transport_window_id,
                                 const base::UnguessableToken& token,
                                 uint32_t embed_flags,
                                 EmbedUsingTokenCallback callback) {
  DVLOG(3) << "EmbedUsingToken transport_window_id="
           << MakeClientWindowId(transport_window_id).ToString()
           << " token=" << token.ToString();
  aura::Window* window =
      GetWindowByClientId(MakeClientWindowId(transport_window_id));
  if (!window) {
    DVLOG(1) << "EmbedUsingToken failed (no window)";
    std::move(callback).Run(false);
    return;
  }

  // Check for a client registered using ScheduleEmbed().
  std::set<WindowTree*> visited_trees;
  mojom::WindowTreeClientPtr client =
      GetAndRemoveScheduledEmbedWindowTreeClient(token, &visited_trees);
  if (client) {
    Embed(transport_window_id, std::move(client), embed_flags,
          std::move(callback));
    return;
  }

  // No client found using ScheduleEmbed(), check for a call to
  // ScheduleEmbedForExistingClient().
  WindowService::TreeAndWindowId tree_and_id =
      window_service_->FindTreeWithScheduleEmbedForExistingClient(token);
  if (!tree_and_id.tree) {
    DVLOG(1) << "EmbedUsingToken failed (token not found)";
    std::move(callback).Run(false);
    return;
  }
  if (tree_and_id.tree == this) {
    DVLOG(1) << "EmbedUsingToken failed, attempt to embed self, token="
             << token.ToString();
    std::move(callback).Run(false);
    return;
  }

  if (!IsClientCreatedWindow(window) || IsTopLevel(window)) {
    DVLOG(1) << "EmbedUsingToken failed (access denied)";
    std::move(callback).Run(false);
    return;
  }

  ServerWindow* server_window = ServerWindow::GetMayBeNull(window);
  const bool owner_intercept_events =
      (connection_type_ != ConnectionType::kEmbedding &&
       (embed_flags & mojom::kEmbedFlagEmbedderInterceptsEvents) != 0);
  tree_and_id.tree->CompleteScheduleEmbedForExistingClient(
      window, tree_and_id.id, token);
  std::unique_ptr<Embedding> embedding =
      std::make_unique<Embedding>(this, window, owner_intercept_events);
  embedding->InitForEmbedInExistingTree(tree_and_id.tree);
  server_window->SetEmbedding(std::move(embedding));
  // Convert |transport_window_id| to ensure the client is supplied a consistent
  // client-id.
  window_tree_client_->OnFrameSinkIdAllocated(
      ClientWindowIdToTransportId(MakeClientWindowId(transport_window_id)),
      server_window->frame_sink_id());
  std::move(callback).Run(true);
}

void WindowTree::SetFocus(uint32_t change_id, Id transport_window_id) {
  window_tree_client_->OnChangeCompleted(
      change_id, SetFocusImpl(MakeClientWindowId(transport_window_id)));
}

void WindowTree::SetCanFocus(Id transport_window_id, bool can_focus) {
  focus_handler_.SetCanFocus(GetWindowByTransportId(transport_window_id),
                             can_focus);
}

void WindowTree::SetCursor(uint32_t change_id,
                           Id transport_window_id,
                           ui::CursorData cursor) {
  window_tree_client_->OnChangeCompleted(
      change_id,
      SetCursorImpl(MakeClientWindowId(transport_window_id), cursor));
}

void WindowTree::SetWindowTextInputState(Id window_id,
                                         ::ui::mojom::TextInputStatePtr state) {
  aura::Window* window = GetWindowByTransportId(window_id);
  if (!window) {
    DVLOG(1) << "SetWindowTextInputState failed (no window)";
    return;
  }
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetWindowTextInputState failed (access denied)";
    return;
  }

  window_service_->delegate()->UpdateTextInputState(window, std::move(state));
}

void WindowTree::SetImeVisibility(Id window_id,
                                  bool visible,
                                  ::ui::mojom::TextInputStatePtr state) {
  aura::Window* window = GetWindowByTransportId(window_id);
  if (!window) {
    DVLOG(1) << "SetImeVisibility failed (no window)";
    return;
  }
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "SetImeVisibility failed (access denied)";
    return;
  }

  window_service_->delegate()->UpdateImeVisibility(window, visible,
                                                   std::move(state));
}

void WindowTree::SetEventTargetingPolicy(Id transport_window_id,
                                         mojom::EventTargetingPolicy policy) {
  aura::Window* window = GetWindowByTransportId(transport_window_id);
  if (IsClientCreatedWindow(window) || IsClientRootWindow(window))
    window->SetEventTargetingPolicy(policy);
}

void WindowTree::OnWindowInputEventAck(uint32_t event_id,
                                       mojom::EventResult result) {
  DVLOG(4) << "OnWindowInputEventAck id=" << event_id;
  std::unique_ptr<ui::Event> key_event;
  if (!in_flight_key_events_.empty() &&
      in_flight_key_events_.front()->id == event_id) {
    key_event = std::move(in_flight_key_events_.front()->event);
    in_flight_key_events_.pop();
  } else if (!in_flight_other_events_.empty() &&
             in_flight_other_events_.front()->id == event_id) {
    DVLOG_IF(1, in_flight_other_events_.front()->event) << "Unexpected event";
    in_flight_other_events_.pop();
  } else {
    DVLOG(1) << "client acked unknown event";
    return;
  }

  if (key_event && result == mojom::EventResult::UNHANDLED) {
    window_service_->delegate()->OnUnhandledKeyEvent(
        *(key_event->AsKeyEvent()));
  }

  for (WindowServiceObserver& observer : window_service_->observers())
    observer.OnClientAckedEvent(client_id_, event_id);
}

void WindowTree::DeactivateWindow(Id transport_window_id) {
  DVLOG(3) << "DeactivateWindow id="
           << MakeClientWindowId(transport_window_id).ToString();
  aura::Window* window = GetWindowByTransportId(transport_window_id);
  if (!window) {
    DVLOG(1) << "DeactivateWindow failed (no window)";
    return;
  }

  if (!IsClientCreatedWindow(window) || !IsTopLevel(window)) {
    DVLOG(1) << "DeactivateWindow failed (access denied)";
    return;
  }

  wm::ActivationClient* activation_client =
      wm::GetActivationClient(window->GetRootWindow());
  if (!activation_client) {
    DVLOG(1) << "DeactivateWindow failed (no activation client)";
    return;
  }

  // Only allow deactivation if |window| is the active window.
  if (activation_client->GetActiveWindow() != window) {
    DVLOG(1) << "DeactivateWindow failed (window is not active)";
    return;
  }

  activation_client->DeactivateWindow(window);
}

void WindowTree::StackAbove(uint32_t change_id, Id above_id, Id below_id) {
  const bool result = StackAboveImpl(MakeClientWindowId(above_id),
                                     MakeClientWindowId(below_id));
  window_tree_client_->OnChangeCompleted(change_id, result);
}

void WindowTree::StackAtTop(uint32_t change_id, Id window_id) {
  const bool result = StackAtTopImpl(MakeClientWindowId(window_id));
  window_tree_client_->OnChangeCompleted(change_id, result);
}

void WindowTree::BindWindowManagerInterface(
    const std::string& name,
    mojom::WindowManagerAssociatedRequest window_manager) {
  auto wm_interface = window_service_->delegate()->CreateWindowManagerInterface(
      this, name, window_manager.PassHandle());
  if (wm_interface)
    window_manager_interfaces_.push_back(std::move(wm_interface));
}

void WindowTree::GetCursorLocationMemory(
    GetCursorLocationMemoryCallback callback) {
  auto shared_buffer_handle =
      aura::Env::GetInstance()->GetLastMouseLocationMemory();
  DCHECK(shared_buffer_handle.is_valid());
  std::move(callback).Run(std::move(shared_buffer_handle));
}

void WindowTree::PerformWindowMove(uint32_t change_id,
                                   Id transport_window_id,
                                   mojom::MoveLoopSource source,
                                   const gfx::Point& cursor) {
  DVLOG(3) << "PerformWindowMove id="
           << MakeClientWindowId(transport_window_id).ToString();
  aura::Window* window = GetWindowByTransportId(transport_window_id);
  if (!IsClientCreatedWindow(window) || !IsTopLevel(window) ||
      !window->IsVisible() || window_moving_) {
    DVLOG(1) << "PerformWindowMove failed (invalid window)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }

  if (source == mojom::MoveLoopSource::MOUSE &&
      !window->env()->IsMouseButtonDown()) {
    DVLOG(1) << "PerformWindowMove failed (mouse not down)";
    window_tree_client_->OnChangeCompleted(change_id, false);
    return;
  }

  window_moving_ = window;
  window_service_->delegate()->RunWindowMoveLoop(
      window, source, cursor,
      base::BindOnce(&WindowTree::OnPerformWindowMoveDone,
                     weak_factory_.GetWeakPtr(), change_id));
}

void WindowTree::CancelWindowMove(Id transport_window_id) {
  if (!window_moving_) {
    DVLOG(1) << "CancelWindowMove called and a move is not underway";
    return;
  }

  aura::Window* window = GetWindowByTransportId(transport_window_id);
  if (window == window_moving_)
    window_service_->delegate()->CancelWindowMoveLoop();
  else
    DVLOG(1) << "CancelWindowMove called with wrong window";
}

void WindowTree::PerformDragDrop(
    uint32_t change_id,
    Id source_window_id,
    const gfx::Point& screen_location,
    const base::flat_map<std::string, std::vector<uint8_t>>& drag_data,
    const gfx::ImageSkia& drag_image,
    const gfx::Vector2d& drag_image_offset,
    uint32_t drag_operation,
    ::ui::mojom::PointerKind source) {
  if (pending_drag_source_window_id_ != kInvalidTransportId) {
    DVLOG(1) << "PerformDragDrop failed (only one drag allowed)";
    window_tree_client_->OnPerformDragDropCompleted(change_id, false,
                                                    mojom::kDropEffectNone);
    return;
  }

  pending_drag_source_window_id_ = source_window_id;

  // Runs the drag loop as a posted task to unwind mojo call stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&WindowTree::DoPerformDragDrop, weak_factory_.GetWeakPtr(),
                     change_id, source_window_id, screen_location, drag_data,
                     drag_image, drag_image_offset, drag_operation, source));
}

void WindowTree::CancelDragDrop(Id window_id) {
  if (pending_drag_source_window_id_ == kInvalidTransportId) {
    DVLOG(1) << "CancelDragDrop called and a drag is not underway";
    return;
  }

  if (pending_drag_source_window_id_ != window_id) {
    DVLOG(1) << "CancelDragDrop called with wrong window";
    return;
  }

  // Clear |pending_drag_source_window_id_| to cancel posted drag loop task.
  pending_drag_source_window_id_ = kInvalidTransportId;

  // Cancel the current drag loop if it is running.
  window_service_->delegate()->CancelDragLoop(
      GetWindowByTransportId(window_id));
}

void WindowTree::ObserveTopmostWindow(mojom::MoveLoopSource source,
                                      Id window_id) {
  if (connection_type_ == ConnectionType::kEmbedding) {
    DVLOG(1) << "ObserveTopmostWindow failed (access denied)";
    return;
  }
  DVLOG(3) << "ObserveTopmostWindow id="
           << MakeClientWindowId(window_id).ToString();
  aura::Window* window = GetWindowByTransportId(window_id);
  if (!IsClientCreatedWindow(window) || !IsTopLevel(window) ||
      !window->IsVisible() || topmost_window_observer_) {
    DVLOG(1) << "ObserveTopmostWindow failed (invalid window)";
    return;
  }

  topmost_window_observer_ =
      std::make_unique<TopmostWindowObserver>(this, source, window);
}

void WindowTree::StopObservingTopmostWindow() {
  if (connection_type_ == ConnectionType::kEmbedding) {
    DVLOG(1) << "StopObservingTopmostWindow failed (access denied)";
    return;
  }
  if (!topmost_window_observer_) {
    DVLOG(1) << "StopObservingTopmostWindow failed";
    return;
  }
  topmost_window_observer_.reset();
}

void WindowTree::CancelActiveTouchesExcept(Id not_cancelled_window_id) {
  if (connection_type_ == ConnectionType::kEmbedding) {
    DVLOG(1) << "CancelActiveTouchesExcept failed (access denied)";
    return;
  }
  DVLOG(3) << "CancelActiveTouchesExcept not_cancelled_window_id="
           << MakeClientWindowId(not_cancelled_window_id).ToString();
  aura::Window* not_cancelled_window = nullptr;
  if (not_cancelled_window_id != kInvalidTransportId) {
    not_cancelled_window = GetWindowByTransportId(not_cancelled_window_id);
    if (!not_cancelled_window || !IsClientCreatedWindow(not_cancelled_window)) {
      DVLOG(1) << "CancelActiveTouchesExcept failed (invalid window)";
      return;
    }
  }
  window_service_->env()->gesture_recognizer()->CancelActiveTouchesExcept(
      not_cancelled_window);
}

void WindowTree::CancelActiveTouches(Id window_id) {
  if (connection_type_ == ConnectionType::kEmbedding) {
    DVLOG(1) << "CancelActiveTouches failed (access denied)";
    return;
  }
  DVLOG(3) << "CancelActiveTouches window_id="
           << MakeClientWindowId(window_id).ToString();
  aura::Window* window = GetWindowByTransportId(window_id);
  if (!window || !IsClientCreatedWindow(window)) {
    DVLOG(1) << "CancelActiveTouches failed (invalid window)";
    return;
  }
  window_service_->env()->gesture_recognizer()->CancelActiveTouches(window);
}

void WindowTree::TransferGestureEventsTo(Id current_id,
                                         Id new_id,
                                         bool should_cancel) {
  if (connection_type_ == ConnectionType::kEmbedding) {
    DVLOG(1) << "TransferGestureEventsTo failed (access denied)";
    return;
  }
  DVLOG(3) << "TransferGestureEventsTo current_id="
           << MakeClientWindowId(current_id).ToString()
           << " new_id=" << MakeClientWindowId(new_id)
           << " should_cancel=" << should_cancel;
  aura::Window* current_window = GetWindowByTransportId(current_id);
  aura::Window* new_window = GetWindowByTransportId(new_id);
  if (!current_window || !IsClientCreatedWindow(current_window)) {
    DVLOG(1) << "TransferGestureEventsTo failed (invalid current_window)";
    return;
  }
  if (!new_window || !IsClientCreatedWindow(new_window)) {
    DVLOG(1) << "TransferGestureEventsTo failed (invalid new_window)";
    return;
  }
  window_service_->env()->gesture_recognizer()->TransferEventsTo(
      current_window, new_window,
      should_cancel ? ui::TransferTouchesBehavior::kCancel
                    : ui::TransferTouchesBehavior::kDontCancel);
}

void WindowTree::TrackOcclusionState(Id transport_window_id) {
  aura::Window* window = GetWindowByTransportId(transport_window_id);
  if (!window) {
    DVLOG(1) << "TrackOcclusionState failed (no window)";
    return;
  }
  if (!IsClientCreatedWindow(window)) {
    DVLOG(1) << "TrackOcclusionState failed (access denied)";
    return;
  }

  window->TrackOcclusionState();
}

void WindowTree::PauseWindowOcclusionTracking() {
  window_occlusion_tracking_pauses_.emplace_back(
      std::make_unique<aura::WindowOcclusionTracker::ScopedPause>(
          window_service_->env()));
}

void WindowTree::UnpauseWindowOcclusionTracking() {
  if (window_occlusion_tracking_pauses_.empty()) {
    DVLOG(1) << "Unbalanced UnpauseWindowOcclusionTracking call.";
    return;
  }

  window_occlusion_tracking_pauses_.pop_back();
}

}  // namespace ws
