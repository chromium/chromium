// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_READER_H_
#define STORAGE_BROWSER_BLOB_BLOB_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/completion_once_callback.h"
#include "services/network/public/cpp/data_pipe_to_source_stream.h"
#include "storage/browser/blob/blob_storage_constants.h"

class GURL;

namespace base {
class FilePath;
class TaskRunner;
class Time;
}

namespace net {
class DrainableIOBuffer;
class IOBuffer;
}

namespace storage {
class BlobDataItem;
class BlobDataHandle;
class BlobDataSnapshot;
class FileStreamReader;

// The blob reader is used to read a blob.  This can only be used in the browser
// process, and we need to be on the IO thread.
//  * There can only be one read happening at a time per reader.
//  * If a status of Status::NET_ERROR is returned, that means there was an
//    error and the net_error() variable contains the error code.
// Use a BlobDataHandle to create an instance.
//
// For more information on how to read Blobs in your specific situation, see:
// https://chromium.googlesource.com/chromium/src/+/HEAD/storage/browser/blob/README.md#how-to-use-blobs-browser_side-accessing-reading
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobReader {
 public:
  class COMPONENT_EXPORT(STORAGE_BROWSER) FileStreamReaderProvider {
   public:
    virtual ~FileStreamReaderProvider();

    virtual std::unique_ptr<FileStreamReader> CreateForLocalFile(
        base::TaskRunner* task_runner,
        const base::FilePath& file_path,
        int64_t initial_offset,
        const base::Time& expected_modification_time) = 0;

    virtual std::unique_ptr<FileStreamReader> CreateFileStreamReader(
        const GURL& filesystem_url,
        int64_t offset,
        int64_t max_bytes_to_read,
        const base::Time& expected_modification_time) = 0;
  };
  enum class Status { NET_ERROR, IO_PENDING, DONE };
  using StatusCallback = base::OnceCallback<void(Status)>;
  virtual ~BlobReader();

  // This calculates the total size of the blob, and initializes the reading
  // cursor.
  //  * This should only be called once per reader.
  //  * Status::Done means that the total_size() value is populated and you can
  //    continue to SetReadRange or Read.
  //  * The 'done' callback is only called if Status::IO_PENDING is returned.
  //    The callback value contains the error code or net::OK. Please use the
  //    total_size() value to query the blob size, as it's uint64_t.
  Status CalculateSize(net::CompletionOnceCallback done);

  // Returns true when the blob has side data. CalculateSize must be called
  // beforehand. Currently side data is supported only for single readable
  // DataHandle entry blob. So it returns false when the blob has more than
  // single data item. This side data is used to pass the V8 code cache which is
  // stored as a side stream in the CacheStorage to the renderer.
  // (crbug.com/581613).  This will still return true even after TakeSideData
  // has been called.
  bool has_side_data() const;

  // Reads the side data of the blob. CalculateSize must be called beforehand.
  // * Always calls the StatusCallback when the side data has been read.
  // * This may be done synchronously or asynchronously.
  // * The done callback will be called with Status::DONE or Status::NET_ERROR.
  // * If the callback returns NET_ERROR, net_error() will have the value.
  // Currently side data is supported only for single readable DataHandle entry
  // blob.
  void ReadSideData(StatusCallback done);

  // Passes the side data (if any) from ReadSideData() to the caller.
  base::Optional<mojo_base::BigBuffer> TakeSideData();

  // Used to set the read position.
  // * This should be called after CalculateSize and before Read.
  // * Range can only be set once.
  Status SetReadRange(uint64_t position, uint64_t length);

  // Reads a portion of the data.
  // * CalculateSize (and optionally SetReadRange) must be called beforehand.
  // * bytes_read is populated only if Status::DONE is returned. Otherwise the
  //   bytes read (or error code) is populated in the 'done' callback.
  // * The done callback is only called if Status::IO_PENDING is returned.
  // * This method can be called multiple times. A bytes_read value (either from
  //   the callback for Status::IO_PENDING or the bytes_read value for
  //   Status::DONE) of 0 means we're finished reading.
  Status Read(net::IOBuffer* buffer,
              size_t dest_size,
              int* bytes_read,
              net::CompletionOnceCallback done);

  // Returns if this reader contains a single MojoDataItem.  If so,
  // ReadSingleMojoDataItem can be called instead of multiple Reads as an
  // optimized path.  This can only be called after CalculateSize.
  bool IsSingleMojoDataItem() const;
  void ReadSingleMojoDataItem(mojo::ScopedDataPipeProducerHandle producer,
                              net::CompletionOnceCallback done);

  // Kills reading and invalidates all callbacks. The reader cannot be used
  // after this call.
  void Kill();

  // Returns if all of the blob's items are in memory. Should only be called
  // after CalculateSize.
  bool IsInMemory() const;

  // Returns the remaining bytes to be read in the blob. This is populated
  // after CalculateSize, and is modified by SetReadRange.
  uint64_t remaining_bytes() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return remaining_bytes_;
  }

  // Returns the net error code if there was an error. Defaults to net::OK.
  int net_error() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return net_error_;
  }

  // Returns the total size of the blob. This is populated after CalculateSize
  // is called.
  uint64_t total_size() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(total_size_calculated_);
    return total_size_;
  }

 protected:
  friend class BlobDataHandle;
  friend class BlobReaderTest;
  FRIEND_TEST_ALL_PREFIXES(BlobReaderTest, HandleBeforeAsyncCancel);
  FRIEND_TEST_ALL_PREFIXES(BlobReaderTest, ReadFromIncompleteBlob);

  BlobReader(const BlobDataHandle* blob_handle);

  bool total_size_calculated() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return total_size_calculated_;
  }

  void SetFileStreamProviderForTesting(
      std::unique_ptr<FileStreamReaderProvider> file_stream_provider) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    file_stream_provider_for_testing_ = std::move(file_stream_provider);
  }

 private:
  Status ReportError(int net_error);
  void InvalidateCallbacksAndDone(int net_error,
                                  net::CompletionOnceCallback done);

  void AsyncCalculateSize(net::CompletionOnceCallback done, BlobStatus status);
  // If this returns Status::IO_PENDING, it will take ownership of the callback
  // pointed at by |done| and call it when complete. |done| will not be
  // modified, otherwise.
  Status CalculateSizeImpl(net::CompletionOnceCallback* done);
  bool AddItemLength(size_t index, uint64_t length);
  bool ResolveFileItemLength(const BlobDataItem& item,
                             int64_t total_length,
                             uint64_t* output_length);
  void DidGetFileItemLength(size_t index, int64_t result);
  void DidCountSize();

  // For reading the blob.
  // Returns if we're done, PENDING_IO if we're waiting on async.
  Status ReadLoop(int* bytes_read);
  // Called from asynchronously called methods to continue the read loop.
  void ContinueAsyncReadLoop();
  // PENDING_IO means we're waiting on async.
  Status ReadItem();
  void AdvanceItem();
  void AdvanceBytesRead(int result);
  void ReadBytesItem(const BlobDataItem& item, int bytes_to_read);
  BlobReader::Status ReadFileItem(FileStreamReader* reader, int bytes_to_read);
  void DidReadFile(int result);
  void DeleteItemReaders();
  Status ReadReadableDataHandle(const BlobDataItem& item, int bytes_to_read);
  void DidReadReadableDataHandle(int result);
  void DidReadItem(int result);
  void DidReadSideData(StatusCallback done,
                       int expected_size,
                       int result,
                       mojo_base::BigBuffer data);
  int ComputeBytesToRead() const;
  int BytesReadCompleted();

  // Returns a FileStreamReader for a blob item at |index|.
  // If the item at |index| is not of type kFile this returns nullptr.
  FileStreamReader* GetOrCreateFileReaderAtIndex(size_t index);
  // If the reader is null, then this basically performs a delete operation.
  void SetFileReaderAtIndex(size_t index,
                            std::unique_ptr<FileStreamReader> reader);
  // Creates a FileStreamReader for the item with additional_offset.
  std::unique_ptr<FileStreamReader> CreateFileStreamReader(
      const BlobDataItem& item,
      uint64_t additional_offset);
  // Returns a DataPipeToSourceStream for a blob item at |Index|.
  // If the item at |index| is not of type kReadableDataHandle this returns
  // nullptr.
  network::DataPipeToSourceStream* GetOrCreateDataPipeAtIndex(size_t index);
  void SetDataPipeAtIndex(
      size_t index,
      std::unique_ptr<network::DataPipeToSourceStream> pipe);
  std::unique_ptr<network::DataPipeToSourceStream> CreateDataPipe(
      const BlobDataItem& item,
      uint64_t additional_offset);

  void RecordBytesReadFromDataHandle(int item_index, int result);

  std::unique_ptr<BlobDataHandle> blob_handle_;
  std::unique_ptr<BlobDataSnapshot> blob_data_;
  std::unique_ptr<FileStreamReaderProvider> file_stream_provider_for_testing_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  base::Optional<mojo_base::BigBuffer> side_data_;

  int net_error_;
  bool item_list_populated_ = false;
  std::vector<uint64_t> item_length_list_;

  scoped_refptr<net::DrainableIOBuffer> read_buf_;

  bool total_size_calculated_ = false;
  uint64_t total_size_ = 0;
  uint64_t remaining_bytes_ = 0;
  size_t pending_get_file_info_count_ = 0;
  std::map<size_t, std::unique_ptr<FileStreamReader>> index_to_reader_;
  std::map<size_t, std::unique_ptr<network::DataPipeToSourceStream>>
      index_to_pipe_;
  size_t current_item_index_ = 0;
  uint64_t current_item_offset_ = 0;

  bool io_pending_ = false;

  net::CompletionOnceCallback size_callback_;
  net::CompletionOnceCallback read_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BlobReader> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(BlobReader);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_READER_H_
