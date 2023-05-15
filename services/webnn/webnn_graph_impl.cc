// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_graph_impl.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace webnn {

WebNNGraphImpl::WebNNGraphImpl() = default;

WebNNGraphImpl::~WebNNGraphImpl() = default;

// static
void WebNNGraphImpl::Create(mojo::PendingReceiver<mojom::WebNNGraph> receiver) {
  mojo::MakeSelfOwnedReceiver<mojom::WebNNGraph>(
      std::make_unique<WebNNGraphImpl>(), std::move(receiver));
}

}  // namespace webnn
