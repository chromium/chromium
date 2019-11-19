// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_BLOB_CLIENT_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_BLOB_CLIENT_LIST_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"

namespace blink {

// This class holds a list of BlobReaderClient implementations alive until
// they complete or the entire list is garbage collected.
class CacheStorageBlobClientList
    : public GarbageCollected<CacheStorageBlobClientList> {
 public:
  CacheStorageBlobClientList() = default;
  void AddClient(
      mojo::PendingReceiver<mojom::blink::BlobReaderClient>
          client_pending_receiver,
      DataPipeBytesConsumer::CompletionNotifier* completion_notifier);

  void Trace(blink::Visitor* visitor);

 private:
  class Client;

  void RevokeClient(Client* client);

  HeapVector<Member<Client>> clients;
  DISALLOW_COPY_AND_ASSIGN(CacheStorageBlobClientList);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_CACHE_STORAGE_BLOB_CLIENT_LIST_H_
