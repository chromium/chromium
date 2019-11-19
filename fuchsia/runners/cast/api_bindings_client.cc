// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/api_bindings_client.h"

#include <utility>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/string_piece.h"

namespace {

uint64_t kBindingsIdStart = 0xFF0000;

}  // namespace

ApiBindingsClient::ApiBindingsClient(
    fidl::InterfaceHandle<chromium::cast::ApiBindings> bindings_service,
    base::OnceClosure on_bindings_received_callback,
    base::OnceClosure on_error_callback)
    : bindings_service_(bindings_service.Bind()),
      on_bindings_received_callback_(std::move(on_bindings_received_callback)) {
  DCHECK(bindings_service_);
  DCHECK(on_bindings_received_callback_);

  bindings_service_->GetAll(
      fit::bind_member(this, &ApiBindingsClient::OnBindingsReceived));

  bindings_service_.set_error_handler(
      [on_error_callback =
           std::move(on_error_callback)](zx_status_t status) mutable {
        ZX_LOG(ERROR, status) << "ApiBindings dicsonnected.";
        std::move(on_error_callback).Run();
      });
}

void ApiBindingsClient::OnBindingsReceived(
    std::vector<chromium::cast::ApiBinding> bindings) {
  DCHECK(on_bindings_received_callback_);

  bindings_ = std::move(bindings);
  std::move(on_bindings_received_callback_).Run();
}

void ApiBindingsClient::AttachToFrame(fuchsia::web::Frame* frame,
                                      NamedMessagePortConnector* connector,
                                      base::OnceClosure on_error_callback) {
  DCHECK(!frame_) << "AttachToFrame() was called twice.";
  DCHECK(frame);
  DCHECK(connector);
  DCHECK(bindings_)
      << "AttachToFrame() was called before bindings were received.";

  connector_ = connector;
  frame_ = frame;

  bindings_service_.set_error_handler([on_error_callback =
                                           std::move(on_error_callback)](
                                          zx_status_t status) mutable {
    ZX_LOG_IF(ERROR, status != ZX_OK, status) << "ApiBindings disconnected.";
    std::move(on_error_callback).Run();
  });

  connector_->Register(base::BindRepeating(&ApiBindingsClient::OnPortConnected,
                                           base::Unretained(this)));

  // Enumerate and inject all scripts in |bindings|.
  uint64_t bindings_id = kBindingsIdStart;
  for (chromium::cast::ApiBinding& entry : *bindings_) {
    frame_->AddBeforeLoadJavaScript(
        bindings_id++, {"*"}, std::move(*entry.mutable_before_load_script()),
        [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
          CHECK(result.is_response()) << "JavaScript injection error: "
                                      << static_cast<uint32_t>(result.err());
        });
  }
}

ApiBindingsClient::~ApiBindingsClient() {
  if (connector_ && frame_) {
    connector_->Register({});

    // Remove all injected scripts using their automatically enumerated IDs.
    for (uint64_t i = 0; i < bindings_->size(); ++i)
      frame_->RemoveBeforeLoadJavaScript(kBindingsIdStart + i);
  }
}

void ApiBindingsClient::OnPortConnected(
    base::StringPiece port_name,
    fidl::InterfaceHandle<fuchsia::web::MessagePort> port) {
  if (bindings_service_)
    bindings_service_->Connect(port_name.as_string(), std::move(port));
}

bool ApiBindingsClient::HasBindings() const {
  return bindings_.has_value();
}
