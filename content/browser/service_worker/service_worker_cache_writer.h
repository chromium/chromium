// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_WRITER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_WRITER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace crypto {
class SecureHash;
}  // namespace crypto

namespace content {

// This class is responsible for possibly updating the ServiceWorker script
// cache for an installed ServiceWorker main script. If there is no existing
// cache entry, this class always writes supplied data back to the cache; if
// there is an existing cache entry, this class only writes supplied data back
// if there is a cache mismatch.
//
// Note that writes done by this class cannot be "short" - ie, if they succeed,
// they always write all the supplied data back. Therefore completions are
// signalled with net::Error without a count of bytes written.
//
// This class's behavior is modelled as a state machine; see the DoLoop function
// for comments about this.
class CONTENT_EXPORT ServiceWorkerCacheWriter {
 public:
  using OnWriteCompleteCallback = base::OnceCallback<void(net::Error)>;

  // These values indicates the timing when the checksum update happens. As the
  // sha256 checksum is a hash string calculated from script data, basically
  // |kCacheMismatch| is preferable in terms of the efficiency.
  //
  // kCacheMismatch: Update the checksum when a cache mismatch and write data
  // back to the cache.
  // kAlways: Update the checksum regardless of whether there is a cache
  // mismatch or not.
  enum class ChecksumUpdateTiming { kCacheMismatch, kAlways };

  // This class defines the interfaces of observer that observes write
  // operations. The observer is notified when response head or data
  // will be written to storage.
  class WriteObserver {
   public:
    // Called before response headers are written to storage.
    // Returns net::OK if success. Other values are treated as errors.
    virtual int WillWriteResponseHead(
        const network::mojom::URLResponseHead& response_head) = 0;

    // Called before response data is written to storage.
    // Return value is used by cache writer to decide what to do next. A net
    // error code should be returned (e.g. net::OK, net::ERR_IO_PENDING). If it
    // returns net::ERR_IO_PENDING, the cache writer waits until the callback
    // is called asynchronously. Otherwise the callback should not be called.
    // The parameter of the callback specifies result of the operation.
    virtual int WillWriteData(
        scoped_refptr<net::IOBuffer> data,
        int length,
        base::OnceCallback<void(net::Error)> callback) = 0;
  };

  // Create a cache writer instance that copies a script already in storage. The
  // script is read by |copy_reader|.
  static std::unique_ptr<ServiceWorkerCacheWriter> CreateForCopy(
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
      int64_t writer_resource_id);

  // Create a cache writer instance that unconditionally write back data
  // supplied to |MaybeWriteHeaders| and |MaybeWriteData| to storage.
  static std::unique_ptr<ServiceWorkerCacheWriter> CreateForWriteBack(
      mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
      int64_t writer_resource_id);

  // Create a cache writer that compares between a script in storage and data
  // from network (supplied with |MaybeWriteHeaders| and |MaybeWriteData|).
  // Nothing would be written to storage if it compares to be identical.
  // When |pause_when_not_identical| is true and the cache writer detects a
  // difference between bodies from the network and from the storage, the
  // comparison stops immediately and the cache writer is paused and returns
  // net::ERR_IO_PENDING, with nothing written to the storage. It can be
  // resumed later. If |pause_when_not_identical| is false, and the data is
  // different, it would be written to storage directly. |copy_reader| is used
  // for copying identical data blocks during writing.
  static std::unique_ptr<ServiceWorkerCacheWriter> CreateForComparison(
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
      int64_t writer_resource_id,
      bool pause_when_not_identical,
      ChecksumUpdateTiming checksum_update_timing);

  ~ServiceWorkerCacheWriter();

  // Writes the supplied |response_head| back to the cache. Returns
  // ERR_IO_PENDING if the write will complete asynchronously, in which case
  // |callback| will be called when it completes. Otherwise, returns a code
  // other than ERR_IO_PENDING and does not invoke |callback|. Note that this
  // method will not necessarily write data back to the cache if the incoming
  // data is equivalent to the existing cached data. See the source of this
  // function for details about how this function drives the state machine.
  net::Error MaybeWriteHeaders(network::mojom::URLResponseHeadPtr response_head,
                               OnWriteCompleteCallback callback);

  // Writes the supplied body data |data| back to the cache. Returns
  // ERR_IO_PENDING if the write will complete asynchronously, in which case
  // |callback| will be called when it completes. Otherwise, returns a code
  // other than ERR_IO_PENDING and does not invoke |callback|. Note that this
  // method will not necessarily write data back to the cache if the incoming
  // data is equivalent to the existing cached data. See the source of this
  // function for details about how this function drives the state machine.
  net::Error MaybeWriteData(net::IOBuffer* buf,
                            size_t buf_size,
                            OnWriteCompleteCallback callback);

  // Returns a count of bytes written back to the cache.
  size_t bytes_written() const { return bytes_written_; }
  bool did_replace() const { return did_replace_; }
  bool is_pausing() const { return state_ == STATE_PAUSING; }

  // Resumes a cache writer which were paused when a block of data from the
  // network wasn't identical to the data in the storage. It is valid to call
  // this method only when |pause_when_not_identical| is true in the constructor
  // and |state_| is STATE_PAUSING.
  net::Error Resume(OnWriteCompleteCallback callback);

  // Start to copy a script in storage to a new position. |callback| is
  // called when the work is done. This is used when an installed script
  // is used by a new service worker with no content change, thus downloading
  // could be avoided.
  net::Error StartCopy(OnWriteCompleteCallback callback);

  // Returns true when the cache writer is created by CreateForCopy().
  bool IsCopying() const;

  // Returns the resource ID being written to storage.
  int64_t writer_resource_id() const;

  void set_write_observer(WriteObserver* write_observer) {
    write_observer_ = write_observer;
  }

  void FlushRemotesForTesting();

  // Gets the hex-encoded checksum hash string calculated from the script body.
  // This function should be called only once as it destroys the underlying data
  // of the checksum. It resets |checksum_| not to be called multiple times.
  std::string GetSha256Checksum();

  ChecksumUpdateTiming checksum_update_timing() const {
    return checksum_update_timing_;
  }

 private:
  class ReadResponseHeadCallbackAdapter;
  class DataPipeReader;

  friend class ServiceWorkerUpdateCheckTestUtils;

  // States for the state machine.
  //
  // The state machine flows roughly like this: if there is no existing cache
  // entry, incoming headers and data are written directly back to the cache
  // ("passthrough mode", the PASSTHROUGH states). If there is an existing cache
  // entry, incoming headers and data are compared to the existing cache entry
  // ("compare mode", the COMPARE states); if at any point the incoming
  // headers/data are not equal to the cached headers/data, this class copies
  // the cached data up to the point where the incoming data and the cached data
  // diverged ("copy mode", the COPY states), then switches to "passthrough
  // mode" to write the remainder of the incoming data. The overall effect is to
  // avoid rewriting the cache entry if the incoming data is identical to the
  // cached data.
  //
  // Note that after a call to MaybeWriteHeaders or MaybeWriteData completes,
  // the machine is always in STATE_DONE, indicating that the call is finished;
  // those methods are responsible for setting a new initial state.
  enum State {
    STATE_START,
    // Control flows linearly through these four states, then loops from
    // READ_DATA_FOR_COMPARE_DONE to READ_DATA_FOR_COMPARE, or exits to
    // READ_HEADERS_FOR_COPY.
    STATE_READ_HEADERS_FOR_COMPARE,
    STATE_READ_HEADERS_FOR_COMPARE_DONE,
    STATE_READ_DATA_FOR_COMPARE,
    STATE_READ_DATA_FOR_COMPARE_DONE,

    // The cache writer is paused because the network data wasn't identical with
    // the stored data, and |pause_when_not_identical| is true.
    STATE_PAUSING,

    // Control flows linearly through these states, with each pass from
    // READ_DATA_FOR_COPY to WRITE_DATA_FOR_COPY_DONE copying one block of data
    // at a time. Control loops from WRITE_DATA_FOR_COPY_DONE back to
    // READ_DATA_FOR_COPY if there is more data to copy. If there is no more
    // data, it exits to WRITE_DATA_FOR_PASSTHROUGH in case IsCopying()
    // returns false or exits to DONE in case IsCopying() returns true.
    STATE_READ_HEADERS_FOR_COPY,
    STATE_READ_HEADERS_FOR_COPY_DONE,
    STATE_WRITE_HEADERS_FOR_COPY,
    STATE_WRITE_HEADERS_FOR_COPY_DONE,
    STATE_READ_DATA_FOR_COPY,
    STATE_READ_DATA_FOR_COPY_DONE,
    STATE_WRITE_DATA_FOR_COPY,
    STATE_WRITE_DATA_FOR_COPY_DONE,

    // Control flows linearly through these states, with a loop between
    // WRITE_DATA_FOR_PASSTHROUGH and WRITE_DATA_FOR_PASSTHROUGH_DONE.
    STATE_WRITE_HEADERS_FOR_PASSTHROUGH,
    STATE_WRITE_HEADERS_FOR_PASSTHROUGH_DONE,
    STATE_WRITE_DATA_FOR_PASSTHROUGH,
    STATE_WRITE_DATA_FOR_PASSTHROUGH_DONE,

    // This state means "done with the current call; ready for another one."
    STATE_DONE,
  };

  ServiceWorkerCacheWriter(
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader,
      mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer,
      int64_t writer_resource_id,
      bool pause_when_not_identical,
      ChecksumUpdateTiming checksum_update_timing);

  // Drives this class's state machine. This function steps the state machine
  // until one of:
  //   a) One of the state functions returns an error
  //   b) The state machine reaches STATE_DONE
  // A successful value (net::OK or greater) indicates that the requested
  // operation completed synchronously. A return value of ERR_IO_PENDING
  // indicates that some step had to submit asynchronous IO for later
  // completion, and the state machine will resume running (via AsyncDoLoop)
  // when that asynchronous IO completes. Any other return value indicates that
  // the requested operation failed synchronously.
  int DoLoop(int result);

  // State handlers. See function comments in the corresponding source file for
  // details on these.
  int DoStart(int result);
  int DoReadHeadersForCompare(int result);
  int DoReadHeadersForCompareDone(int result);
  int DoReadDataForCompare(int result);
  int DoReadDataForCompareDone(int result);
  int DoReadHeadersForCopy(int result);
  int DoReadHeadersForCopyDone(int result);
  int DoWriteHeadersForCopy(int result);
  int DoWriteHeadersForCopyDone(int result);
  int DoReadDataForCopy(int result);
  int DoReadDataForCopyDone(int result);
  int DoWriteDataForCopy(int result);
  int DoWriteDataForCopyDone(int result);
  int DoWriteHeadersForPassthrough(int result);
  int DoWriteHeadersForPassthroughDone(int result);
  int DoWriteDataForPassthrough(int result);
  int DoWriteDataForPassthroughDone(int result);
  int DoDone(int result);

  // Wrappers for asynchronous calls. These are responsible for scheduling a
  // callback to drive the state machine if needed. These return ERR_IO_PENDING,
  // and schedule a callback to run the state machine's Run() later.
  int ReadResponseHead(storage::mojom::ServiceWorkerResourceReader* reader);
  int ReadDataHelper(storage::mojom::ServiceWorkerResourceReader* reader,
                     std::unique_ptr<DataPipeReader>& data_pipe_reader,
                     scoped_refptr<net::IOBuffer> buf,
                     int buf_len);

  // If no write observer is set through set_write_observer(),
  // WriteResponseHead() operates the same as
  // WriteResponseHeadToResponseWriter() and WriteData() operates the same as
  // WriteDataToResponseWriter().
  // If observer is set, the argument |response_head| or |data| is first sent
  // to observer then WriteResponseHeadToResponseWriter() or
  // WriteDataToResponseWriter() is called.
  int WriteResponseHead(network::mojom::URLResponseHeadPtr response_head);
  int WriteData(scoped_refptr<net::IOBuffer> data, size_t length);
  int WriteResponseHeadToResponseWriter(
      network::mojom::URLResponseHeadPtr response_head);
  int WriteDataToResponseWriter(scoped_refptr<net::IOBuffer> data,
                                size_t length);

  // Called when |write_observer_| finishes its WillWriteData() operation.
  void OnWillWriteDataCompleted(scoped_refptr<net::IOBuffer> data,
                                size_t length,
                                net::Error error);

  void OnRemoteDisconnected();

  // Callback used by the above helpers for their IO operations. This is only
  // run when those IO operations complete asynchronously, in which case it
  // invokes the synchronous DoLoop function and runs the client callback (the
  // one passed into MaybeWriteData/MaybeWriteHeaders) if that invocation
  // of DoLoop completes synchronously.
  void AsyncDoLoop(int result);

  State state_;
  // Note that this variable is only used for assertions; it reflects "state !=
  // DONE && not in synchronous DoLoop".
  bool io_pending_;
  bool comparing_;

  network::mojom::URLResponseHeadPtr response_head_to_read_;
  network::mojom::URLResponseHeadPtr response_head_to_write_;
  scoped_refptr<net::IOBuffer> data_to_read_;
  int len_to_read_;
  scoped_refptr<net::IOBuffer> data_to_copy_;
  scoped_refptr<net::IOBuffer> data_to_write_;
  int len_to_write_;
  OnWriteCompleteCallback pending_callback_;

  size_t cached_length_;

  // The amount of data from the network (|data_to_write_|) which has already
  // been compared with data from storage (|data_to_read_|). This is
  // initialized to 0 for every new arrival of network data.
  size_t compare_offset_;

  // Count of bytes which has been read from the network for comparison, and
  // known as identical with the stored scripts. It is incremented only when a
  // full block of network data is compared, to avoid having to use only
  // fragments of the buffered network data.
  size_t bytes_compared_;

  // The total size of the body for copying. Used only when IsCopying() returns
  // true.
  size_t bytes_to_copy_ = 0;

  // Count of bytes copied from |copy_reader_| to |writer_|.
  size_t bytes_copied_;

  // Count of bytes written back to |writer_|.
  size_t bytes_written_;

  bool did_replace_ = false;

  // When the cache writer finds any differences between bodies from the network
  // and from the storage, and the |pause_when_not_identical_| is true, the
  // cache writer pauses immediately.
  const bool pause_when_not_identical_;

  raw_ptr<WriteObserver, DanglingUntriaged> write_observer_ = nullptr;

  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> compare_reader_;
  std::unique_ptr<DataPipeReader> compare_data_pipe_reader_;
  mojo::Remote<storage::mojom::ServiceWorkerResourceReader> copy_reader_;
  std::unique_ptr<DataPipeReader> copy_data_pipe_reader_;

  mojo::Remote<storage::mojom::ServiceWorkerResourceWriter> writer_;
  const int64_t writer_resource_id_ =
      blink::mojom::kInvalidServiceWorkerResourceId;

  // Normally, the sha256 hash string is calculated only when there is an update
  // on the script. But if |checksum_update_timing_| is kAlways, the hash
  // string is calculated even when there is no update in the script.
  const ChecksumUpdateTiming checksum_update_timing_;

  // Calculate the hash string for the written bytes. This will be used for the
  // experiment which needs to identify some specific service worker scripts
  // (crbug.com/1371756).
  std::unique_ptr<crypto::SecureHash> checksum_;

  base::WeakPtrFactory<ServiceWorkerCacheWriter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_WRITER_H_
