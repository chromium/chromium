// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBINSTANCE_HOST_FUCHSIA_WEB_DEBUG_PROXY_H_
#define FUCHSIA_WEB_WEBINSTANCE_HOST_FUCHSIA_WEB_DEBUG_PROXY_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_ptr_set.h>

// Proxies the fuchsia.web.Debug protocol to one or more registered
// fuchsia.web.Debug implementations, allowing clients to use a single
// fuchsia.web.Debug instance to observe DevTools availability across
// multiple components (e.g. multiple web instances).
class FuchsiaWebDebugProxy final : public fuchsia::web::Debug,
                                   public fuchsia::web::DevToolsListener {
 public:
  explicit FuchsiaWebDebugProxy();
  ~FuchsiaWebDebugProxy() override;

  // Returns true if one or more clients are active.
  bool has_clients() const { return devtools_listeners_.size() != 0u; }

  // Registers a new fuchsia.web.Debug protocol to be proxied.
  void RegisterInstance(fidl::InterfaceHandle<fuchsia::web::Debug> debug);

  // fuchsia::web::Debug implementation.
  void EnableDevTools(
      fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
      EnableDevToolsCallback callback) override;

 private:
  // fuchsia::web::DevToolsListener implementation.
  void OnContextDevToolsAvailable(
      fidl::InterfaceRequest<fuchsia::web::DevToolsPerContextListener> request)
      override;

  // DevToolsListeners registered by clients via the Debug interface.
  fidl::InterfacePtrSet<fuchsia::web::DevToolsListener> devtools_listeners_;

  // DevToolsListener bindings, connected to active web instances.
  fidl::BindingSet<fuchsia::web::DevToolsListener> instance_bindings_;
};

#endif  //  FUCHSIA_WEB_WEBINSTANCE_HOST_FUCHSIA_WEB_DEBUG_PROXY_H_
