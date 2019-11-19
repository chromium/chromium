// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_API_BINDINGS_CLIENT_H_
#define FUCHSIA_RUNNERS_CAST_API_BINDINGS_CLIENT_H_

#include <fuchsia/web/cpp/fidl.h>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"

// Injects scripts received from the ApiBindings service, and provides connected
// ports to the Agent.
class ApiBindingsClient {
 public:
  // Reads bindings definitions from |bindings_service_| at construction time.
  // Invokes |on_bindings_received_callback| once the definitions are received,
  // after which the client may invoke AttachToFrame().
  ApiBindingsClient(
      fidl::InterfaceHandle<chromium::cast::ApiBindings> bindings_service,
      base::OnceClosure on_bindings_received_callback,
      base::OnceClosure on_error_callback);
  ~ApiBindingsClient();

  // Injects APIs and handles channel connections on |frame|.
  // |on_error_closure|: Invoked in the event of an unrecoverable error (e.g.
  //                     lost connection to the Agent). The callback must
  //                     remain valid for the entire lifetime of |this|.
  void AttachToFrame(fuchsia::web::Frame* frame,
                     NamedMessagePortConnector* connector,
                     base::OnceClosure on_error_callback);

  // Indicates that bindings were successfully received from
  // |bindings_service_|.
  bool HasBindings() const;

 private:
  // Called when ApiBindings::GetAll() has responded.
  void OnBindingsReceived(std::vector<chromium::cast::ApiBinding> bindings);

  // Called when |connector_| has connected a port.
  void OnPortConnected(base::StringPiece port_name,
                       fidl::InterfaceHandle<fuchsia::web::MessagePort> port);

  base::Optional<std::vector<chromium::cast::ApiBinding>> bindings_;
  fuchsia::web::Frame* frame_ = nullptr;
  NamedMessagePortConnector* connector_ = nullptr;
  chromium::cast::ApiBindingsPtr bindings_service_;
  base::OnceClosure on_bindings_received_callback_;

  DISALLOW_COPY_AND_ASSIGN(ApiBindingsClient);
};

#endif  // FUCHSIA_RUNNERS_CAST_API_BINDINGS_CLIENT_H_
