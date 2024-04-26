// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_API_BINDINGS_CLIENT_H_
#define FUCHSIA_WEB_RUNNERS_CAST_API_BINDINGS_CLIENT_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/cast/message_port/message_port.h"
#include "components/cast/named_message_port_connector/named_message_port_connector.h"

// Injects scripts received from the ApiBindings service, and provides connected
// ports to the Agent.
class ApiBindingsClient {
 public:
  // Reads bindings definitions from |bindings_service_| at construction time.
  // |on_initialization_complete| is invoked when either the initial bindings
  // have been received, or on failure. The caller should use HasBindings()
  // to verify that bindings were received, and may then use AttachToFrame().
  ApiBindingsClient(
      fidl::InterfaceHandle<chromium::cast::ApiBindings> bindings_service,
      base::OnceClosure on_initialization_complete);

  ApiBindingsClient(const ApiBindingsClient&) = delete;
  ApiBindingsClient& operator=(const ApiBindingsClient&) = delete;

  ~ApiBindingsClient();

  // Injects APIs and handles channel connections on |frame|.
  // |on_error_callback| is invoked asynchronusly in the event of an
  // unrecoverable error (e.g. lost connection to the Agent). The callback must
  // remain valid for the entire lifetime of |this|.
  void AttachToFrame(fuchsia::web::Frame* frame,
                     cast_api_bindings::NamedMessagePortConnector* connector,
                     base::OnceClosure on_error_callback);

  // Indicates that the Frame is no longer live, preventing the API bindings
  // client from attempting to remove injected bindings from it.
  void DetachFromFrame(fuchsia::web::Frame* frame);

  // Indicates that bindings were successfully received from
  // |bindings_service_|.
  bool HasBindings() const;

  // TODO(crbug.com/40131115): Move this method back to private once the Cast
  // Streaming Receiver component has been implemented.
  // Called when |connector_| has connected a port.
  bool OnPortConnected(std::string_view port_name,
                       std::unique_ptr<cast_api_bindings::MessagePort> port);

 private:
  // Called when ApiBindings::GetAll() has responded.
  void OnBindingsReceived(std::vector<chromium::cast::ApiBinding> bindings);

  // Used by AttachToFrame() to invoke `on_error_callback` asynchronously.
  void CallOnErrorCallback(base::OnceClosure on_error_callback);

  std::optional<std::vector<chromium::cast::ApiBinding>> bindings_;
  fuchsia::web::Frame* frame_ = nullptr;
  cast_api_bindings::NamedMessagePortConnector* connector_ = nullptr;
  chromium::cast::ApiBindingsPtr bindings_service_;
  base::OnceClosure on_initialization_complete_;

  base::WeakPtrFactory<ApiBindingsClient> weak_ptr_factory_{this};
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_API_BINDINGS_CLIENT_H_
