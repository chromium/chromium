// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_fuchsia.h"

#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/process/process.h"
#include "base/threading/thread_checker.h"
#include "base/types/expected.h"
#include "net/base/fuchsia/network_interface_cache.h"

namespace net {

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(bool require_wlan)
    : NetworkChangeNotifierFuchsia(internal::ConnectInterfacesWatcher(),
                                   require_wlan,
                                   /*system_dns_config_notifier=*/nullptr) {}

NetworkChangeNotifierFuchsia::NetworkChangeNotifierFuchsia(
    fuchsia::net::interfaces::WatcherHandle watcher_handle,
    bool require_wlan,
    SystemDnsConfigChangeNotifier* system_dns_config_notifier)
    : NetworkChangeNotifier(NetworkChangeCalculatorParams(),
                            system_dns_config_notifier),
      cache_(require_wlan) {
  DCHECK(watcher_handle);

  std::vector<fuchsia::net::interfaces::Properties> interfaces;
  auto handle_or_status = internal::ReadExistingNetworkInterfacesFromNewWatcher(
      std::move(watcher_handle), interfaces);
  if (!handle_or_status.has_value()) {
    ZX_LOG(ERROR, handle_or_status.error()) << "ReadExistingNetworkInterfaces";
    base::Process::TerminateCurrentProcessImmediately(1);
  }

  HandleCacheStatus(cache_.AddInterfaces(std::move(interfaces)));

  watcher_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia.net.interfaces.Watcher"));
  zx_status_t bind_status = watcher_.Bind(std::move(handle_or_status.value()));
  ZX_CHECK(bind_status == ZX_OK, bind_status) << "Bind()";
  watcher_->Watch(
      fit::bind_member(this, &NetworkChangeNotifierFuchsia::OnInterfacesEvent));
}

NetworkChangeNotifierFuchsia::~NetworkChangeNotifierFuchsia() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ClearGlobalPointer();
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierFuchsia::GetCurrentConnectionType() const {
  return cache_.GetConnectionType();
}

const internal::NetworkInterfaceCache*
NetworkChangeNotifierFuchsia::GetNetworkInterfaceCacheInternal() const {
  return &cache_;
}

void NetworkChangeNotifierFuchsia::OnInterfacesEvent(
    fuchsia::net::interfaces::Event event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Immediately trigger the next watch, which will happen asynchronously. If
  // event processing encounters an error it'll close the watcher channel which
  // will cancel any pending callbacks.
  watcher_->Watch(
      fit::bind_member(this, &NetworkChangeNotifierFuchsia::OnInterfacesEvent));

  switch (event.Which()) {
    case fuchsia::net::interfaces::Event::kAdded:
      HandleCacheStatus(cache_.AddInterface(std::move(event.added())));
      break;
    case fuchsia::net::interfaces::Event::kRemoved:
      HandleCacheStatus(cache_.RemoveInterface(event.removed()));
      break;
    case fuchsia::net::interfaces::Event::kChanged:
      HandleCacheStatus(cache_.ChangeInterface(std::move(event.changed())));
      break;
    default:
      LOG(ERROR) << "Unexpected event: " << event.Which();
      watcher_.Unbind();
      cache_.SetError();
      break;
  }
}

void NetworkChangeNotifierFuchsia::HandleCacheStatus(
    std::optional<internal::NetworkInterfaceCache::ChangeBits> change_bits) {
  if (!change_bits.has_value()) {
    watcher_.Unbind();
    return;
  }

  if (change_bits.value() &
      internal::NetworkInterfaceCache::kIpAddressChanged) {
    NotifyObserversOfIPAddressChange();
  }
  if (change_bits.value() &
      internal::NetworkInterfaceCache::kConnectionTypeChanged) {
    NotifyObserversOfConnectionTypeChange();
  }
}

namespace internal {

fuchsia::net::interfaces::WatcherHandle ConnectInterfacesWatcher() {
  fuchsia::net::interfaces::StateSyncPtr state;
  zx_status_t status =
      base::ComponentContextForProcess()->svc()->Connect(state.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect()";

  // GetWatcher() is a feed-forward API, so failures will be observed via
  // peer-closed events on the returned `watcher`.
  fuchsia::net::interfaces::WatcherHandle watcher;
  status = state->GetWatcher(/*options=*/{}, watcher.NewRequest());

  return watcher;
}

base::expected<fuchsia::net::interfaces::WatcherHandle, zx_status_t>
ReadExistingNetworkInterfacesFromNewWatcher(
    fuchsia::net::interfaces::WatcherHandle watcher_handle,
    std::vector<fuchsia::net::interfaces::Properties>& interfaces) {
  DCHECK(watcher_handle);

  fuchsia::net::interfaces::WatcherSyncPtr watcher = watcher_handle.BindSync();

  // fuchsia.net.interfaces.Watcher implements a hanging-get pattern, accepting
  // a single Watch() call and returning an event when something changes.
  // When a Watcher is first created, it emits a series of events describing
  // existing interfaces, terminated by an "idle" event, before entering the
  // normal hanging-get flow.
  while (true) {
    fuchsia::net::interfaces::Event event;
    if (auto watch_status = watcher->Watch(&event); watch_status != ZX_OK) {
      ZX_LOG(ERROR, watch_status) << "Watch() failed";
      return base::unexpected(watch_status);
    }

    switch (event.Which()) {
      case fuchsia::net::interfaces::Event::Tag::kExisting:
        interfaces.push_back(std::move(event.existing()));
        break;
      case fuchsia::net::interfaces::Event::Tag::kIdle:
        // Idle means we've listed all the existing interfaces. We can stop
        // fetching events.
        return base::ok(watcher.Unbind());
      default:
        LOG(ERROR) << "Unexpected event " << event.Which();
        return base::unexpected(ZX_ERR_BAD_STATE);
    }
  }
}

}  // namespace internal
}  // namespace net
