// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_IMPL_H_
#define STORAGE_BROWSER_BLOB_BLOB_IMPL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace storage {

class BlobDataHandle;

// Self destroys when no more bindings exist.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobImpl
    : public blink::mojom::Blob,
      public network::mojom::DataPipeGetter {
 public:
  static base::WeakPtr<BlobImpl> Create(
      std::unique_ptr<BlobDataHandle> handle,
      mojo::PendingReceiver<blink::mojom::Blob> receiver);

  // blink::mojom::Blob:
  void Clone(mojo::PendingReceiver<blink::mojom::Blob> receiver) override;
  void AsDataPipeGetter(
      mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) override;
  void ReadRange(
      uint64_t offset,
      uint64_t length,
      mojo::ScopedDataPipeProducerHandle handle,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override;
  void ReadAll(
      mojo::ScopedDataPipeProducerHandle handle,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client) override;
  void ReadSideData(ReadSideDataCallback callback) override;
  void GetInternalUUID(GetInternalUUIDCallback callback) override;

  // network::mojom::DataPipeGetter:
  void Clone(
      mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) override;
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override;

  void FlushForTesting();

 private:
  BlobImpl(std::unique_ptr<BlobDataHandle> handle,
           mojo::PendingReceiver<blink::mojom::Blob> receiver);
  ~BlobImpl() override;
  void OnMojoDisconnect();

  std::unique_ptr<BlobDataHandle> handle_;

  mojo::ReceiverSet<blink::mojom::Blob> receivers_;
  mojo::ReceiverSet<network::mojom::DataPipeGetter> data_pipe_getter_receivers_;

  base::WeakPtrFactory<BlobImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BlobImpl);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_IMPL_H_
