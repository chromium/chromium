// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/direct_sockets_service_mojo_remote.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
DirectSocketsServiceMojoRemote* DirectSocketsServiceMojoRemote::Create(
    ExecutionContext* execution_context,
    base::OnceClosure disconnect_handler) {
  auto* service =
      MakeGarbageCollected<DirectSocketsServiceMojoRemote>(execution_context);

  execution_context->GetBrowserInterfaceBroker().GetInterface(
      service->get().BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kNetworking)));
  service->get().set_disconnect_handler(std::move(disconnect_handler));

  DCHECK(service->get().is_bound());

  return service;
}

DirectSocketsServiceMojoRemote::DirectSocketsServiceMojoRemote(
    ExecutionContext* execution_context)
    : service_{execution_context} {}

DirectSocketsServiceMojoRemote::~DirectSocketsServiceMojoRemote() = default;

void DirectSocketsServiceMojoRemote::Close() {
  service_.reset();
}

void DirectSocketsServiceMojoRemote::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
}

}  // namespace blink
