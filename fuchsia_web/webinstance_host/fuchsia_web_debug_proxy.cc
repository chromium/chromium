// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webinstance_host/fuchsia_web_debug_proxy.h"

#include <lib/fidl/cpp/binding.h>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

namespace {

// Proxies notifications from a DevToolsPerContextListener channel connected
// to a web container, to the specified set of DevToolsListeners.
class PerContextListenerProxy final
    : public fuchsia::web::DevToolsPerContextListener {
 public:
  PerContextListenerProxy(
      fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> request,
      const fidl::InterfacePtrSet<fuchsia::web::DevToolsListener>& clients)
      : binding_(this, std::move(request)) {
    // Notify clients of the newly-available per-Context DevTools.
    for (const auto& client : clients.ptrs()) {
      fidl::InterfaceHandle<fuchsia::web::DevToolsPerContextListener> handle;
      (*client)->OnContextDevToolsAvailable(handle.NewRequest());
      auto ptr = std::make_unique<
          fidl::InterfacePtr<fuchsia::web::DevToolsPerContextListener>>(
          handle.Bind());

      // If the client disconnects then remove it. If no clients remain then
      // delete this proxy.
      ptr->set_error_handler([this, client = ptr.get()](zx_status_t status) {
        ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status);
        clients_.erase(client);
        if (clients_.empty())
          delete this;
      });

      clients_.insert(std::move(ptr));
    }

    // Delete this proxy if the instance goes-away.
    binding_.set_error_handler([this](zx_status_t status) {
      ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status);
      delete this;
    });
  }
  ~PerContextListenerProxy() override = default;

  // fuchsia::web::DevToolsPerContextListener implementation.
  void OnHttpPortOpen(uint16_t port) override {
    for (const auto& client : clients_) {
      (*client)->OnHttpPortOpen(port);
    }
  }

 private:
  fidl::Binding<fuchsia::web::DevToolsPerContextListener> binding_;

  // Connections to clients to which to propagate updates.
  base::flat_set<std::unique_ptr<fidl::InterfacePtr<
                     fuchsia::web::DevToolsPerContextListener>>,
                 base::UniquePtrComparator>
      clients_;
};

}  // namespace

FuchsiaWebDebugProxy::FuchsiaWebDebugProxy() = default;
FuchsiaWebDebugProxy::~FuchsiaWebDebugProxy() = default;

void FuchsiaWebDebugProxy::EnableDevTools(
    fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
    EnableDevToolsCallback callback) {
  // Add the listener to the active set, but do not inform it of any debuggable
  // instances that already exist, since callers are expected to only start
  // creating new instances after the callback has returned.
  devtools_listeners_.AddInterfacePtr(listener.Bind());

  // If there were statically-configured fuchsia.web.Debug instances configured
  // then they should be connected-to here, and |callback()| invoked only
  // when each static instance' |EnableDevTools()| call had completed, or
  // failed.
  callback();
}

void FuchsiaWebDebugProxy::RegisterInstance(
    fidl::InterfaceHandle<fuchsia::web::Debug> debug) {
  DCHECK(has_clients());

  // Create a new DevToolsListener binding, and connect it to the instance.
  fidl::InterfaceHandle<fuchsia::web::DevToolsListener> handle;
  instance_bindings_.AddBinding(this, handle.NewRequest());
  debug.Bind()->EnableDevTools(std::move(handle), {});
}

void FuchsiaWebDebugProxy::OnContextDevToolsAvailable(
    fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> request) {
  // Create a proxy to propagate notifications for this new per-Context
  // listener out to each of the clients.
  new PerContextListenerProxy(std::move(request), devtools_listeners_);
}
