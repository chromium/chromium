// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_MOJO_BLOB_READER_H_
#define STORAGE_BROWSER_BLOB_MOJO_BLOB_READER_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "net/http/http_byte_range.h"
#include "storage/browser/blob/blob_reader.h"

namespace network {
class NetToMojoPendingBuffer;
}

namespace storage {
class BlobDataHandle;

// Reads a blob into a data pipe. Owns itself, and owns its delegate. Self
// destructs when reading is complete.
class COMPONENT_EXPORT(STORAGE_BROWSER) MojoBlobReader {
 public:
  // Methods on this delegate are called in the order they are defined here.
  // With the exception of DidRead, each method is called at most once.
  // OnComplete is always called, and is always the last method to be called,
  // every other method could be skipped in case of errors, or if the method is
  // otherwise not applicable.
  class Delegate {
   public:
    enum RequestSideData { REQUEST_SIDE_DATA, DONT_REQUEST_SIDE_DATA };

    virtual ~Delegate() = default;

    // Called when the blob being read has been fully constructed and its size
    // is known. |total_size| is the total size of the blob, while
    // |content_size| is the size of the subset of this blob that matches the
    // range passed to Create.
    // Return |REQUEST_SIDE_DATA| if the blob's side data should be returned,
    // otherwise MojoBlobReader will skip reading side data and immediately
    // start reading the actual blob contents. Side data is used for example by
    // service worker code to store compiled javascript alongside the script
    // source in the cache.
    virtual RequestSideData DidCalculateSize(uint64_t total_size,
                                             uint64_t content_size) = 0;

    // Called if DidCalculateSize returned |REQUEST_SIDE_DATA|, with the side
    // data associated with the blob being read, if any.
    virtual void DidReadSideData(std::optional<mojo_base::BigBuffer> data) {}

    // Called whenever some amount of data is read from the blob and about to be
    // written to the data pipe.
    virtual void DidRead(int num_bytes) {}

    // Called when reading the blob has finished. If an error occurs this could
    // be the only method that gets called, but either way this method is always
    // the last to be called, shortly before the delegate is deleted.
    // |total_written_bytes| indicated the total number of bytes that were read
    // from the blob, and written to the data pipe. When successful this should
    // always be equal to the |content_size| that was passed to
    // DidCalculateSize.
    virtual void OnComplete(net::Error result,
                            uint64_t total_written_bytes) = 0;
  };

  static void Create(const BlobDataHandle* handle,
                     const net::HttpByteRange& range,
                     std::unique_ptr<Delegate> delegate,
                     mojo::ScopedDataPipeProducerHandle response_body_stream);

  MojoBlobReader(const MojoBlobReader&) = delete;
  MojoBlobReader& operator=(const MojoBlobReader&) = delete;

 private:
  MojoBlobReader(const BlobDataHandle* handle,
                 const net::HttpByteRange& range,
                 std::unique_ptr<Delegate> delegate,
                 mojo::ScopedDataPipeProducerHandle response_body_stream);
  ~MojoBlobReader();

  void Start();

  void NotifyCompletedAndDeleteIfNeeded(int result);

  void DidCalculateSize(int result);
  void DidReadSideData(BlobReader::Status status);
  void StartReading();
  void ReadMore();
  void DidRead(bool completed_synchronously, int num_bytes);
  void OnResponseBodyStreamClosed(MojoResult result,
                                  const mojo::HandleSignalsState& state);
  void OnResponseBodyStreamReady(MojoResult result,
                                 const mojo::HandleSignalsState& state);

  const std::unique_ptr<Delegate> delegate_;

  // The range of the blob that should be read. Could be unbounded if the entire
  // blob is being read.
  net::HttpByteRange byte_range_;

  // Underlying BlobReader data is being read from.
  std::unique_ptr<BlobReader> blob_reader_;

  // Mojo data pipe where the data that is being read is written to. Will be
  // null during write operations, at which time |pending_write_| owns the data
  // pipe instead.
  mojo::ScopedDataPipeProducerHandle response_body_stream_;

  // During a write operation this owns the data pipe handle and gives access to
  // the pipe's internal buffer where data should be written.
  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;

  // Watchers to keep track of the state of the data pipe. One watches for the
  // pipe being writable, while the other watches for the pipe unexpectedly
  // closing.
  mojo::SimpleWatcher writable_handle_watcher_;
  mojo::SimpleWatcher peer_closed_handle_watcher_;

  // Total number of bytes written to the data pipe so far.
  int64_t total_written_bytes_ = 0;

  // Set to true when the delegate's OnComplete has been called. Used to make
  // sure OnComplete isn't called more than once.
  bool notified_completed_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MojoBlobReader> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_MOJO_BLOB_READER_H_
