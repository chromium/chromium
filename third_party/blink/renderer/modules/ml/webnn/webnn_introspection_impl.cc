// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/webnn_introspection_impl.h"

#include "third_party/blink/renderer/platform/wtf/static_constructors.h"

namespace blink {

// static
WebNNIntrospectionImpl& WebNNIntrospectionImpl::GetInstance() {
  DEFINE_STATIC_LOCAL(WebNNIntrospectionImpl, instance, ());
  return instance;
}

// static
void WebNNIntrospectionImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::blink::WebNNIntrospection> receiver) {
  auto& instance = GetInstance();
  // When the browser disables graph recording, it drops its remote. When it
  // (re-)enables it, the browser re-binds the connection. To handle rapid
  // toggling or other edge cases, we unconditionally reset to overwrite any
  // still-existing receiver.
  instance.receiver_.reset();
  instance.receiver_.Bind(std::move(receiver));
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
bool WebNNIntrospectionImpl::IsGraphRecordingEnabled() const {
  return client_.is_bound() && client_.is_connected();
}

void WebNNIntrospectionImpl::SetClient(
    ::mojo::PendingRemote<blink::mojom::blink::WebNNIntrospectionClient>
        client) {
  // When the browser disables graph recording, it drops its remote. When it
  // (re-)enables it, the browser re-binds the connection. To handle rapid
  // toggling or other edge cases, we unconditionally reset to overwrite any
  // still-existing remote.
  client_.reset();
  client_.Bind(std::move(client));
  client_.reset_on_disconnect();
}

void WebNNIntrospectionImpl::OnGraphRecorded(
    ::mojo_base::BigBuffer json_data) const {
  if (client_) {
    client_->OnGraphRecorded(std::move(json_data));
  }
}
#endif

}  // namespace blink
