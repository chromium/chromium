// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/mock_mojo_indexed_db_database_callbacks.h"

namespace content::indexed_db {

MockMojoDatabaseCallbacks::MockMojoDatabaseCallbacks() = default;
MockMojoDatabaseCallbacks::~MockMojoDatabaseCallbacks() = default;

mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
MockMojoDatabaseCallbacks::CreateInterfacePtrAndBind() {
  return receiver_.BindNewEndpointAndPassRemote();
}

mojo::PendingAssociatedRemote<blink::mojom::IDBDatabaseCallbacks>
MockMojoDatabaseCallbacks::BindNewEndpointAndPassDedicatedRemote() {
  return receiver_.BindNewEndpointAndPassDedicatedRemote();
}

void MockMojoDatabaseCallbacks::FlushForTesting() {
  receiver_.FlushForTesting();
}

}  // namespace content::indexed_db
