// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_mojo_indexed_db_callbacks.h"

namespace content {

MockMojoIndexedDBCallbacks::MockMojoIndexedDBCallbacks() = default;
MockMojoIndexedDBCallbacks::~MockMojoIndexedDBCallbacks() = default;

mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks>
MockMojoIndexedDBCallbacks::CreateInterfacePtrAndBind() {
  return receiver_.BindNewEndpointAndPassRemote();
}

}  // namespace content
