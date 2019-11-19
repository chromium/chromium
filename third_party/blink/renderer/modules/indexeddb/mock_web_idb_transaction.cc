// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_transaction.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace blink {

MockWebIDBTransaction::MockWebIDBTransaction(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    int64_t transaction_id) {}

MockWebIDBTransaction::MockWebIDBTransaction() = default;

MockWebIDBTransaction::~MockWebIDBTransaction() = default;

mojo::PendingAssociatedReceiver<mojom::blink::IDBTransaction>
MockWebIDBTransaction::CreateReceiver() {
  return mojo::NullAssociatedReceiver();
}

}  // namespace blink
