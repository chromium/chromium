// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/api_bindings_client.h"

#include <string_view>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"

namespace {

uint64_t kBindingsIdStart = 0xFF0000;

}  // namespace

ApiBindingsClient::ApiBindingsClient(
    fidl::InterfaceHandle<chromium::cast::ApiBindings> bindings_service,
    base::OnceClosure on_initialization_complete)
    : bindings_service_(bindings_service.Bind()),
      on_initialization_complete_(std::move(on_initialization_complete)) {
  DCHECK(bindings_service_);
  DCHECK(on_initialization_complete_);

  bindings_service_->GetAll(
      fit::bind_member(this, &ApiBindingsClient::OnBindingsReceived));

  bindings_service_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ApiBindings disconnected.";
    std::move(on_initialization_complete_).Run();
  });
}

ApiBindingsClient::~ApiBindingsClient() {
  if (connector_ && frame_) {
    connector_->RegisterPortHandler({});

    // Remove all injected scripts using their automatically enumerated IDs.
    for (uint64_t i = 0; i < bindings_->size(); ++i)
      frame_->RemoveBeforeLoadJavaScript(kBindingsIdStart + i);
  }
}

void ApiBindingsClient::AttachToFrame(
    fuchsia::web::Frame* frame,
    cast_api_bindings::NamedMessagePortConnector* connector,
    base::OnceClosure on_error_callback) {
  DCHECK(!frame_) << "AttachToFrame() was called twice.";
  DCHECK(frame);
  DCHECK(connector);

  if (!bindings_service_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ApiBindingsClient::CallOnErrorCallback,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(on_error_callback)));
    return;
  }

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

  connector_->RegisterPortHandler(base::BindRepeating(
      &ApiBindingsClient::OnPortConnected, base::Unretained(this)));

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

void ApiBindingsClient::DetachFromFrame(fuchsia::web::Frame* frame) {
  DCHECK_EQ(frame, frame_);
  frame_ = nullptr;
  bindings_service_.set_error_handler(nullptr);
}

bool ApiBindingsClient::HasBindings() const {
  return bindings_.has_value();
}

bool ApiBindingsClient::OnPortConnected(
    std::string_view port_name,
    std::unique_ptr<cast_api_bindings::MessagePort> port) {
  if (!bindings_service_)
    return false;

  bindings_service_->Connect(
      std::string(port_name),
      cast_api_bindings::MessagePortFuchsia::FromMessagePort(port.get())
          ->TakeClientHandle());
  return true;
}

void ApiBindingsClient::OnBindingsReceived(
    std::vector<chromium::cast::ApiBinding> bindings) {
  bindings_ = std::move(bindings);
  bindings_service_.set_error_handler(nullptr);
  std::move(on_initialization_complete_).Run();
}

void ApiBindingsClient::CallOnErrorCallback(
    base::OnceClosure on_error_callback) {
  std::move(on_error_callback).Run();
}
