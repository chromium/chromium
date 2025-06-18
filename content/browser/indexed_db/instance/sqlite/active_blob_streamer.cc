// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/sqlite/active_blob_streamer.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "content/browser/indexed_db/blob_reader.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/net_adapters.h"

namespace content::indexed_db::sqlite {

namespace {

// Each time a blob is read via mojo, a DataPipe is opened to pass the bytes
// through. This object is created once for each such read and feeds blob bytes
// into the pipe. The object is self owned.
class SqliteBlobToDataPipe {
 public:
  // This version feeds the bytes into `dest` and notifies `client` of important
  // events. It's used when the BlobStorageContext is reading the blob.
  SqliteBlobToDataPipe(
      base::RepeatingCallback<sql::StreamingBlobHandle*(void)>
          readable_blob_handle,
      uint64_t blob_size,
      uint64_t offset,
      uint64_t read_length,
      mojo::ScopedDataPipeProducerHandle dest,
      mojo::PendingRemote<blink::mojom::BlobReaderClient> client)
      : readable_blob_handle_(std::move(readable_blob_handle)),
        dest_(std::move(dest)),
        client_(std::move(client)),
        offset_(offset),
        read_length_(read_length) {
    client_->OnCalculatedSize(blob_size, read_length_);
  }

  // This version feeds the bytes into `dest` and invokes `completion_callback`
  // when done. It's used when a renderer is reading the blob.
  SqliteBlobToDataPipe(base::RepeatingCallback<sql::StreamingBlobHandle*(void)>
                           readable_blob_handle,
                       uint64_t offset,
                       uint64_t read_length,
                       mojo::ScopedDataPipeProducerHandle dest,
                       base::OnceCallback<void(int)> completion_callback)
      : readable_blob_handle_(std::move(readable_blob_handle)),
        dest_(std::move(dest)),
        completion_callback_(std::move(completion_callback)),
        offset_(offset),
        read_length_(read_length) {}

  ~SqliteBlobToDataPipe() = default;

  void Start() {
    if (read_length_ == 0) {
      OnComplete(net::OK);
      return;
    }

    writable_handle_watcher_.emplace(FROM_HERE,
                                     mojo::SimpleWatcher::ArmingPolicy::MANUAL);
    writable_handle_watcher_->Watch(
        dest_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
        base::BindRepeating(&SqliteBlobToDataPipe::OnDataPipeWritable,
                            base::Unretained(this)));
    OnDataPipeWritable(MOJO_RESULT_OK);
  }

 private:
  void OnDataPipeWritable(MojoResult result) {
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      OnComplete(net::ERR_ABORTED);
      return;
    }
    DCHECK_EQ(result, MOJO_RESULT_OK) << result;

    sql::StreamingBlobHandle* readable_blob_handle =
        readable_blob_handle_.Run();
    if (!readable_blob_handle) {
      OnComplete(net::ERR_ABORTED);
      return;
    }

    // This loop shouldn't block the thread for *too* long as the mojo pipe has
    // a capacity of 2MB (i.e. `BeginWrite()` will return
    // MOJO_RESULT_SHOULD_WAIT at some point when streaming a large enough
    // blob).
    while (true) {
      scoped_refptr<network::NetToMojoPendingBuffer> pending_write;
      MojoResult mojo_result =
          network::NetToMojoPendingBuffer::BeginWrite(&dest_, &pending_write);
      switch (mojo_result) {
        case MOJO_RESULT_OK:
          break;
        case MOJO_RESULT_SHOULD_WAIT:
          // The pipe is full. We need to wait for it to have more space.
          writable_handle_watcher_->ArmOrNotify();
          return;
        case MOJO_RESULT_FAILED_PRECONDITION:
          // The data pipe consumer handle has been closed.
          OnComplete(net::ERR_ABORTED);
          return;
        default:
          // The body stream is in a bad state. Bail out.
          OnComplete(net::ERR_UNEXPECTED);
          return;
      }

      size_t read_bytes = base::checked_cast<size_t>(
          std::min(static_cast<uint64_t>(pending_write->size()),
                   read_length_ - transferred_bytes_));
      base::span<uint8_t> buffer =
          base::as_writable_byte_span(*pending_write).first(read_bytes);
      bool success =
          readable_blob_handle->Read(offset_ + transferred_bytes_, buffer);

      if (!success) {
        // Read error.
        dest_ = pending_write->Complete(0);
        OnComplete(net::ERR_FAILED);
        return;
      }

      dest_ = pending_write->Complete(read_bytes);
      transferred_bytes_ += read_bytes;

      if (transferred_bytes_ == read_length_) {
        OnComplete(net::OK);
        return;
      }
      CHECK_LT(transferred_bytes_, read_length_);
    }
  }

  void OnComplete(int result) {
    // Resets the watchers, pipes and the exchange handler, so that
    // we will never be called back.
    if (writable_handle_watcher_) {
      writable_handle_watcher_->Cancel();
    }
    dest_.reset();

    if (client_) {
      client_->OnComplete(result, transferred_bytes_);
    } else {
      std::move(completion_callback_).Run(result);
    }
    delete this;
  }

  // If the database goes away, this getter will return null, and `this` must
  // abort.
  base::RepeatingCallback<sql::StreamingBlobHandle*(void)>
      readable_blob_handle_;

  // `this` is the producer, and `dest_` is its handle to the pipe.
  mojo::ScopedDataPipeProducerHandle dest_;

  // Exactly one of these two members will be non-null.
  mojo::Remote<blink::mojom::BlobReaderClient> client_;
  base::OnceCallback<void(int)> completion_callback_;

  // The number of bytes successfully transferred so far.
  uint64_t transferred_bytes_ = 0;

  // Each read has a certain offset and length specified by the consumer.
  uint64_t offset_;
  uint64_t read_length_ = 0;

  // Optional so that its construction can be deferred.
  std::optional<mojo::SimpleWatcher> writable_handle_watcher_;
};

}  // namespace

void ActiveBlobStreamer::Clone(
    mojo::PendingReceiver<blink::mojom::Blob> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ActiveBlobStreamer::AsDataPipeGetter(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  data_pipe_getter_receivers_.Add(this, std::move(receiver));
}

void ActiveBlobStreamer::ReadRange(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  (new SqliteBlobToDataPipe(base::BindRepeating(
                                [](base::WeakPtr<ActiveBlobStreamer> streamer)
                                    -> sql::StreamingBlobHandle* {
                                  if (!streamer) {
                                    return nullptr;
                                  }
                                  return &streamer->readable_blob_handle_;
                                },
                                weak_factory_.GetWeakPtr()),
                            blob_length_, offset,
                            ClampReadLength(offset, length), std::move(handle),
                            std::move(client)))
      ->Start();
}

void ActiveBlobStreamer::ReadAll(
    mojo::ScopedDataPipeProducerHandle handle,
    mojo::PendingRemote<blink::mojom::BlobReaderClient> client) {
  ReadRange(0, std::numeric_limits<uint64_t>::max(), std::move(handle),
            std::move(client));
}

void ActiveBlobStreamer::Load(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const std::string& method,
    const net::HttpRequestHeaders& headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  // Bounce back to the registry so that we can avoid reimplementing
  // `BlobUrlLoader`. This is used for Object URLs. It's not clear how often
  // this is used or how important it is to make it super efficient.
  registry_blob_->Load(std::move(loader), method, headers, std::move(client));
}

void ActiveBlobStreamer::ReadSideData(
    blink::mojom::Blob::ReadSideDataCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void ActiveBlobStreamer::CaptureSnapshot(CaptureSnapshotCallback callback) {
  // This method is used for the File API. Technically IDB can store Files, but
  // when it does so, the size and last modification date should always be known
  // and propagated to the renderer through IndexedDBExternalObject's metadata.
  // This path is likely only reached when the file modification date and/or
  // size is somehow unknown, but reproducing this scenario has proven
  // difficult. See crbug.com/390586616
  std::move(callback).Run(blob_length_, std::nullopt);
}

void ActiveBlobStreamer::GetInternalUUID(GetInternalUUIDCallback callback) {
  std::move(callback).Run(uuid_);
}

void ActiveBlobStreamer::Clone(
    mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) {
  data_pipe_getter_receivers_.Add(this, std::move(receiver));
}

void ActiveBlobStreamer::Read(
    mojo::ScopedDataPipeProducerHandle pipe,
    network::mojom::DataPipeGetter::ReadCallback on_size_known) {
  std::move(on_size_known).Run(net::OK, blob_length_);
  Read(0, std::numeric_limits<uint64_t>::max(), std::move(pipe),
       base::DoNothing());
}

void ActiveBlobStreamer::Read(
    uint64_t offset,
    uint64_t length,
    mojo::ScopedDataPipeProducerHandle pipe,
    storage::mojom::BlobDataItemReader::ReadCallback callback) {
  (new SqliteBlobToDataPipe(base::BindRepeating(
                                [](base::WeakPtr<ActiveBlobStreamer> streamer)
                                    -> sql::StreamingBlobHandle* {
                                  if (!streamer) {
                                    return nullptr;
                                  }
                                  return &streamer->readable_blob_handle_;
                                },
                                weak_factory_.GetWeakPtr()),
                            offset, ClampReadLength(offset, length),
                            std::move(pipe), std::move(callback)))
      ->Start();
}

void ActiveBlobStreamer::ReadSideData(
    storage::mojom::BlobDataItemReader::ReadSideDataCallback callback) {
  // This type should never have side data.
  std::move(callback).Run(net::ERR_NOT_IMPLEMENTED, mojo_base::BigBuffer());
}

ActiveBlobStreamer::ActiveBlobStreamer(
    const IndexedDBExternalObject& blob_info,
    sql::StreamingBlobHandle readable_blob_handle,
    base::OnceClosure on_became_inactive)
    : uuid_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      blob_length_(blob_info.size()),
      content_type_(base::UTF16ToUTF8(blob_info.type())),
      readable_blob_handle_(std::move(readable_blob_handle)),
      on_became_inactive_(std::move(on_became_inactive)) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &ActiveBlobStreamer::OnMojoDisconnect, base::Unretained(this)));
  data_pipe_getter_receivers_.set_disconnect_handler(base::BindRepeating(
      &ActiveBlobStreamer::OnMojoDisconnect, base::Unretained(this)));
  readers_.set_disconnect_handler(base::BindRepeating(
      &ActiveBlobStreamer::OnMojoDisconnect, base::Unretained(this)));
}

ActiveBlobStreamer::~ActiveBlobStreamer() = default;

void ActiveBlobStreamer::BindRegistryBlob(
    storage::mojom::BlobStorageContext& blob_registry) {
  CHECK(!registry_blob_.is_bound());
  auto element = storage::mojom::BlobDataItem::New();
  element->size = blob_length_;
  element->side_data_size = 0;
  element->content_type = content_type_;
  element->type = storage::mojom::BlobDataItemType::kIndexedDB;
  readers_.Add(this, element->reader.InitWithNewPipeAndPassReceiver());
  blob_registry.RegisterFromDataItem(
      registry_blob_.BindNewPipeAndPassReceiver(), uuid_, std::move(element));
}

void ActiveBlobStreamer::AddReceiver(
    mojo::PendingReceiver<blink::mojom::Blob> receiver,
    storage::mojom::BlobStorageContext& blob_registry) {
  if (!registry_blob_.is_bound()) {
    CHECK(receivers_.empty());
    BindRegistryBlob(blob_registry);
  }
  Clone(std::move(receiver));
}

void ActiveBlobStreamer::OnMojoDisconnect() {
  if (!receivers_.empty() || !data_pipe_getter_receivers_.empty()) {
    return;
  }

  // Unregistering the blob will drop its reference to the `BlobDataItem`
  // associated with `this` as a `BlobDataItemReader`, which will often lead to
  // `readers_` receiving a disconnect. But there may still be other references
  // to the `BlobDataItem`, such as another blob, which means that `this` can go
  // on living indefinitely. See crbug.com/392376370
  registry_blob_.reset();

  if (readers_.empty()) {
    std::move(on_became_inactive_).Run();
    // `this` is deleted.
  }
}

uint64_t ActiveBlobStreamer::ClampReadLength(uint64_t offset,
                                             uint64_t length) const {
  return base::ClampMin(length, base::ClampSub(blob_length_, offset));
}

}  // namespace content::indexed_db::sqlite
