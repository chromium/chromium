// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_BLOB_READER_H_
#define CONTENT_BROWSER_INDEXED_DB_BLOB_READER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content::indexed_db {

// This class reads files from the IDB blob store, which are just plain files on
// disk, and serves them through the mojom Blob interface. It does this in
// multiple ways:
//
// * For the most common case, i.e. reading a blob with blink::mojom::ReadAll(),
//   this class reads from disk and pipes bytes directly to the renderer.
// * In cases where the blob registry needs to read bytes, such as for copying
//   to a local file or to serve data for blob:// URLs, this class reads from
//   disk and returns the bytes via `BlobDataItemReader`.
class BlobReader : public blink::mojom::Blob,
                   public network::mojom::DataPipeGetter,
                   public storage::mojom::BlobDataItemReader {
 public:
  BlobReader(const IndexedDBExternalObject& blob_info,
             base::OnceClosure on_last_receiver_disconnected);
  ~BlobReader() override;

  BlobReader(const BlobReader&) = delete;
  BlobReader& operator=(const BlobReader&) = delete;

  // Like Clone(), but called by the BucketContext (which owns `this`).
  void AddReceiver(mojo::PendingReceiver<blink::mojom::Blob> receiver,
                   storage::mojom::BlobStorageContext& blob_registry);

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
  void Load(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const std::string& method,
      const net::HttpRequestHeaders& headers,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) override;
  void ReadSideData(blink::mojom::Blob::ReadSideDataCallback callback) override;
  void CaptureSnapshot(CaptureSnapshotCallback callback) override;
  void GetInternalUUID(GetInternalUUIDCallback callback) override;

  // network::mojom::DataPipeGetter:
  void Clone(
      mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) override;
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            network::mojom::DataPipeGetter::ReadCallback callback) override;

  // storage::mojom::BlobDataItemReader:
  void Read(uint64_t offset,
            uint64_t length,
            mojo::ScopedDataPipeProducerHandle pipe,
            storage::mojom::BlobDataItemReader::ReadCallback callback) override;
  void ReadSideData(storage::mojom::BlobDataItemReader::ReadSideDataCallback
                        callback) override;

 private:
  void BindRegistryBlob(storage::mojom::BlobStorageContext& blob_registry);
  void OnMojoDisconnect();

  // This UUID is used for both the blob that's served via `blink::mojom::Blob`
  // and the blob in the registry. This is crucial because operations such as
  // copying the blob to a new file do so by identifying the blob to the blob
  // registry using the UUID.
  std::string uuid_;

  // This is the expected length of the file, which comes from the LevelDB
  // record. This acts on a cap on the number of bytes to be read from the file.
  // It can even be zero, in which case even a missing file will be treated as
  // normal (non-error).
  uint64_t blob_length_;

  std::string content_type_;

  const base::FilePath file_path_;

  // Notes on lifetimes:
  //
  // `receivers_` and `data_pipe_getter_receivers_` correspond to mojo
  // connections to the renderer process. When these are both empty,
  // `registry_blob_` will be reset. This *usually* causes the blob registry
  // to drop the other side of the `BlobDataItemReader` (which is owned by a
  // `ShareableBlobDataItem`), which triggers `this` to be destroyed by running
  // `on_last_receiver_disconnected`. However, if that `ShareableBlobDataItem`
  // is in fact shared, as is the case with composed blobs, then it will not
  // drop the other side of the `BlobDataItemReader`. When that happens, `this`
  // will continue living. If the renderer looks up the same blob again,
  // `BucketContext` will reuse this object, and `AddReceiver()` will have to
  // re-establish a placeholder with the blob registry, i.e. re-bind
  // `registry_blob_`.
  mojo::ReceiverSet<blink::mojom::Blob> receivers_;
  mojo::ReceiverSet<network::mojom::DataPipeGetter> data_pipe_getter_receivers_;

  mojo::ReceiverSet<storage::mojom::BlobDataItemReader> readers_;
  mojo::Remote<blink::mojom::Blob> registry_blob_;

  base::OnceClosure on_last_receiver_disconnected_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_BLOB_READER_H_
