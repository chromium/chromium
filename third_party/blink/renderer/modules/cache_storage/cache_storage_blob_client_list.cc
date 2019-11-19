// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage_blob_client_list.h"

namespace blink {

// Class implementing the BlobReaderClient interface.  This is used to
// propagate the completion of an eager body blob read to the
// DataPipeBytesConsumer.
class CacheStorageBlobClientList::Client
    : public GarbageCollected<CacheStorageBlobClientList::Client>,
      public mojom::blink::BlobReaderClient {
  // We must prevent any mojo messages from coming in after this object
  // starts getting garbage collected.
  USING_PRE_FINALIZER(CacheStorageBlobClientList::Client, Dispose);

 public:
  Client(CacheStorageBlobClientList* owner,
         mojo::PendingReceiver<mojom::blink::BlobReaderClient>
             client_pending_receiver,
         DataPipeBytesConsumer::CompletionNotifier* completion_notifier)
      : owner_(owner),
        client_receiver_(this, std::move(client_pending_receiver)),
        completion_notifier_(completion_notifier) {}

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}

  void OnComplete(int32_t status, uint64_t data_length) override {
    client_receiver_.reset();

    // 0 is net::OK
    if (status == 0)
      completion_notifier_->SignalComplete();
    else
      completion_notifier_->SignalError(BytesConsumer::Error());

    if (owner_)
      owner_->RevokeClient(this);
  }

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(owner_);
    visitor->Trace(completion_notifier_);
  }

 private:
  void Dispose() {
    // Use the existence of the client_receiver_ binding to see if this
    // client has already completed.
    if (!client_receiver_.is_bound())
      return;

    client_receiver_.reset();
    completion_notifier_->SignalError(BytesConsumer::Error("aborted"));

    // If we are already being garbage collected its not necessary to
    // call RevokeClient() on the owner.
  }

  WeakMember<CacheStorageBlobClientList> owner_;
  mojo::Receiver<mojom::blink::BlobReaderClient> client_receiver_;
  Member<DataPipeBytesConsumer::CompletionNotifier> completion_notifier_;

  DISALLOW_COPY_AND_ASSIGN(Client);
};

void CacheStorageBlobClientList::AddClient(
    mojo::PendingReceiver<mojom::blink::BlobReaderClient>
        client_pending_receiver,
    DataPipeBytesConsumer::CompletionNotifier* completion_notifier) {
  clients.emplace_back(MakeGarbageCollected<Client>(
      this, std::move(client_pending_receiver), completion_notifier));
}

void CacheStorageBlobClientList::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients);
}

void CacheStorageBlobClientList::RevokeClient(Client* client) {
  auto index = clients.Find(client);
  clients.EraseAt(index);
}

}  // namespace blink
