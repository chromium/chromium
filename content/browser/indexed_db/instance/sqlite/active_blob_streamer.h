// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_ACTIVE_BLOB_STREAMER_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_ACTIVE_BLOB_STREAMER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/blob_storage_context.mojom.h"
#include "content/browser/indexed_db/indexed_db_external_object.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "sql/streaming_blob_handle.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

namespace content::indexed_db::sqlite {

// This class represents an "active" blob, that is, a blob in an IndexedDB
// database which has been vended to one or more clients, and is still connected
// to at least one client. It is owned by a DatabaseConnection and its existence
// is enough to keep the DatabaseConnection alive, since the underlying SQLite
// database connection cannot be closed while any blob is active.
//
// When this class exists, there is a corresponding entry in the
// `blob_references` table.
//
// This class borrows heavily from `indexed_db::BlobReader`, which is used to
// read blobs that are stored as standalone files, and is likely to be
// eventually phased out.
class ActiveBlobStreamer : public blink::mojom::Blob,
                           public network::mojom::DataPipeGetter,
                           public storage::mojom::BlobDataItemReader {
 public:
  ActiveBlobStreamer(
      const IndexedDBExternalObject& blob_info,
      base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
          fetch_blob_chunk,
      int max_chunk_size,
      base::OnceClosure on_became_inactive,
      base::RepeatingCallback<void(net::Error)> on_read_complete);
  ~ActiveBlobStreamer() override;

  ActiveBlobStreamer(const ActiveBlobStreamer&) = delete;
  ActiveBlobStreamer& operator=(const ActiveBlobStreamer&) = delete;

  // Like Clone(), but called by the DatabaseConnection (which owns `this`).
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

  // Clamps `length` to fit within the blob given the starting position
  // `offset`.
  uint64_t ClampReadLength(uint64_t offset, uint64_t length) const;

  // Reads `into.size()` bytes from the blob starting at `offset`, storing them
  // in `into`. Returns true for success, meaning exactly `into.size()` bytes
  // were read, and false otherwise. Even on failure, `into` could be modified.
  // If the requested bytes span multiple chunks, this will handle reading from
  // them all. See `overflow_blob_chunks` in DatabaseConnection for an
  // explanation of chunking.
  bool ReadBlobBytes(uint64_t offset, base::span<uint8_t> into);
  // Called after finishing servicing a single ActiveBlobStreamer::Read(), which
  // happens after 0-many calls to ReadBlobBytes().
  void BlobReadComplete(net::Error result);

  // This UUID is used for both the blob that's served via `blink::mojom::Blob`
  // and the blob in the registry. This is crucial because operations such as
  // copying the blob to a new file do so by identifying the blob to the blob
  // registry using the UUID.
  std::string uuid_;

  // This is the length of the blob, which comes from the SQLite row.
  uint64_t blob_length_;

  // A MIME type.
  std::string content_type_;

  // The handle currently opened for reading. This is a result of
  // `fetch_blob_chunk_`, cached here to avoid extra work when reading from the
  // same chunk multiple times in a row. Technically, `this` could be
  // simultaneously serving multiple `Read()` requests, in which case this
  // caching may have little to no value (it will thrash nearly every time
  // ReadBlobBytes is invoked). However it's expected to be rare that a single
  // blob would be read multiple times simultaneously.
  std::optional<sql::StreamingBlobHandle> readable_blob_handle_;
  // The index of the chunk currently held in `readable_blob_handle_`. Starts as
  // -1 to indicate that no handle has been fetched.
  int chunk_idx_ = -1;

  // Gets a blob chunk by the index of the chunk. It's expected that the chunk
  // will out-last `this`, since `this` is owned by the DatabaseConnection that
  // owns the SQLite DB.
  base::RepeatingCallback<std::optional<sql::StreamingBlobHandle>(size_t)>
      fetch_blob_chunk_;
  // The maximum size of a blob chunk, in bytes.
  const int max_chunk_size_;

  // Notes on lifetimes:
  //
  // `receivers_` and `data_pipe_getter_receivers_` correspond to mojo
  // connections to the renderer process. When these are both empty,
  // `registry_blob_` will be reset. This *usually* causes the blob registry
  // to drop the other side of the `BlobDataItemReader` (which is owned by a
  // `ShareableBlobDataItem`), which triggers `this` to be destroyed by running
  // `on_became_inactive`. However, if that `ShareableBlobDataItem` is in fact
  // shared, as is the case with composed blobs, then it will not drop the other
  // side of the `BlobDataItemReader`. When that happens, `this` will continue
  // living. If the renderer looks up the same blob again, `DatabaseConnection`
  // will reuse this object, and `AddReceiver()` will have to re-establish a
  // placeholder with the blob registry, i.e. re-bind `registry_blob_`.
  mojo::ReceiverSet<blink::mojom::Blob> receivers_;
  mojo::ReceiverSet<network::mojom::DataPipeGetter> data_pipe_getter_receivers_;

  mojo::ReceiverSet<storage::mojom::BlobDataItemReader> readers_;
  mojo::Remote<blink::mojom::Blob> registry_blob_;

  base::OnceClosure on_became_inactive_;

  // Run from `BlobReadComplete()`, i.e., on completion of every attempt to read
  // the contents of the underlying blob.
  base::RepeatingCallback<void(net::Error)> on_read_complete_;

  base::WeakPtrFactory<ActiveBlobStreamer> weak_factory_{this};
};

}  // namespace content::indexed_db::sqlite

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_ACTIVE_BLOB_STREAMER_H_
