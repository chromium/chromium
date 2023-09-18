// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_PRODUCER_H_
#define MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_PRODUCER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/system_export.h"

namespace mojo {

// Helper class which takes ownership of a ScopedDataPipeProducerHandle and
// assumes responsibility for feeding it the contents of a DataSource. This
// takes care of waiting for pipe capacity as needed, and can notify callers
// asynchronously when the operation is complete.
//
// Note that the DataPipeProducer must be kept alive until notified of
// completion to ensure that all of the intended contents are written to the
// pipe. Premature destruction may result in partial or total truncation of data
// made available to the consumer.
class MOJO_CPP_SYSTEM_EXPORT DataPipeProducer {
 public:
  using CompletionCallback = base::OnceCallback<void(MojoResult result)>;
  // Interface definition of abstracted content reader that has minimum
  // base::File equivalent interface to read content.
  class DataSource {
   public:
    // Used as a return value for Read().
    struct ReadResult {
      // The number of bytes read. If returned |bytes_read| is less than
      // requested size, it means EOF is reached.
      uint64_t bytes_read = 0;

      // MojoResult resulting from this call.
      MojoResult result = MOJO_RESULT_OK;
    };
    virtual ~DataSource() {}

    // Returns maximum data size. Actual size can be smaller. Used as a hint to
    // adjust internal read buffer size.
    virtual uint64_t GetLength() const = 0;

    // Similar to base::File::Read(), reads the given number of bytes (or until
    // EOF is reached) starting with the given offset. Returns ReadResult to
    // represent the number of bytes read and errors.
    virtual ReadResult Read(uint64_t offset, base::span<char> buffer) = 0;

    // Notifies DataPipeProducer aborts read operations.
    virtual void Abort() {}
  };

  // Constructs a new DataPipeProducer which will write data to |producer|.
  explicit DataPipeProducer(ScopedDataPipeProducerHandle producer);

  DataPipeProducer(const DataPipeProducer&) = delete;
  DataPipeProducer& operator=(const DataPipeProducer&) = delete;

  ~DataPipeProducer();

  // Attempts to eventually write all of |data_source|'s contents to the pipe.
  // Invokes |callback| asynchronously when done. Note that |callback| IS
  // allowed to delete this DataPipeProducer.
  //
  // If the write is successful |result| will be |MOJO_RESULT_OK|. Otherwise
  // (e.g. if the producer detects the consumer is closed and the pipe has no
  // remaining capacity, or if file open/reads fail for any reason) |result|
  // will be one of the following:
  //
  //   |MOJO_RESULT_ABORTED|
  //   |MOJO_RESULT_NOT_FOUND|
  //   |MOJO_RESULT_PERMISSION_DENIED|
  //   |MOJO_RESULT_RESOURCE_EXHAUSTED|
  //   |MOJO_RESULT_UNKNOWN|
  //
  // Note that if the DataPipeProducer is destroyed before |callback| can be
  // invoked, |callback| is *never* invoked, and the write will be permanently
  // interrupted (and the producer handle closed) after making potentially only
  // partial progress.
  //
  // Multiple writes may be performed in sequence (each one after the last
  // completes), but Write() must not be called before the |callback| for the
  // previous call to Write() (if any) has returned.
  void Write(std::unique_ptr<DataSource> reader, CompletionCallback callback);

  // Returns the underlying producer handle.
  // Must be called only when writes are NOT ongoing.
  const DataPipeProducerHandle& GetProducerHandle() const;

 private:
  class SequenceState;

  void InitializeNewRequest(CompletionCallback callback);
  void OnWriteComplete(CompletionCallback callback,
                       ScopedDataPipeProducerHandle producer,
                       MojoResult result);

  ScopedDataPipeProducerHandle producer_;
  scoped_refptr<SequenceState> sequence_state_;
  base::WeakPtrFactory<DataPipeProducer> weak_factory_{this};
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DATA_PIPE_PRODUCER_H_
