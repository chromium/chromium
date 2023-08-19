// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_mojo_indexed_db_factory_client.h"

namespace content {

MockMojoIndexedDBFactoryClient::MockMojoIndexedDBFactoryClient() = default;
MockMojoIndexedDBFactoryClient::~MockMojoIndexedDBFactoryClient() = default;

mojo::PendingAssociatedRemote<blink::mojom::IDBFactoryClient>
MockMojoIndexedDBFactoryClient::CreateInterfacePtrAndBind() {
  return receiver_.BindNewEndpointAndPassRemote();
}

}  // namespace content
