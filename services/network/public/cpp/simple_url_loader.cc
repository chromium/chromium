// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/public/cpp/simple_url_loader.h"

#include <stdint.h>

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/optional_ref.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

constexpr size_t SimpleURLLoader::kMaxBoundedStringDownloadSize;
constexpr size_t SimpleURLLoader::kMaxUploadStringSizeToCopy;

BASE_FEATURE(kSimpleURLLoaderUseReadAndDiscardBodyOption,
             "SimpleURLLoaderUseReadAndDiscardBodyOption",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr int64_t kReceivedBodySizeUnknown = -1;

// Used by tests to override the tick clock for the timeout timer.
const base::TickClock* timeout_tick_clock_ = nullptr;

// A temporary util adapter to wrap the download callback with the response
// body, and to hop the string content from a unique_ptr<string> into a
// optional<string>.
void GetFromUniquePtrToOptional(
    SimpleURLLoader::BodyAsStringCallback body_as_string_callback,
    std::unique_ptr<std::string> response_body) {
  std::move(body_as_string_callback)
      .Run(response_body
               ? std::make_optional<std::string>(std::move(*response_body))
               : std::nullopt);
}

// This file contains SimpleURLLoaderImpl, several BodyHandler implementations,
// BodyReader, and StringUploadDataPipeGetter.
//
// SimpleURLLoaderImpl implements URLLoaderClient and drives the URLLoader.
//
// Each SimpleURLLoaderImpl creates a BodyHandler when the request is started.
// The BodyHandler drives the body pipe, handles body data as needed (writes it
// to a string, file, etc), and handles passing that data to the consumer's
// callback.
//
// BodyReader is a utility class that BodyHandler implementations use to drive
// the BodyPipe. This isn't handled by the SimpleURLLoader as some BodyHandlers
// consume data off thread, so having it as a separate class allows the data
// pipe to be used off thread, reducing use of the main thread.
//
// StringUploadDataPipeGetter is a class to stream a string upload body to the
// network service, rather than to copy it all at once.

class StringUploadDataPipeGetter : public mojom::DataPipeGetter {
 public:
  StringUploadDataPipeGetter(std::string upload_string,
                             const base::Location& url_loader_created_from)
      : upload_string_(std::move(upload_string)) {}

  StringUploadDataPipeGetter(const StringUploadDataPipeGetter&) = delete;
  StringUploadDataPipeGetter& operator=(const StringUploadDataPipeGetter&) =
      delete;

  ~StringUploadDataPipeGetter() override = default;

  // Returns a mojo::PendingRemote<mojom::DataPipeGetter> for a new upload
  // attempt, closing all previously opened pipes.
  mojo::PendingRemote<mojom::DataPipeGetter> GetRemoteForNewUpload() {
    // If this is a retry, need to close all receivers, since only one consumer
    // can read from the data pipe at a time.
    receiver_set_.Clear();
    // Not strictly needed, but seems best to close the old body pipe and stop
    // any pending reads.
    ResetBodyPipe();

    mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter;
    receiver_set_.Add(this, data_pipe_getter.InitWithNewPipeAndPassReceiver());
    return data_pipe_getter;
  }

 private:
  // DataPipeGetter implementation:

  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override {
    // Close any previous body pipe, to avoid confusion between the two, if the
    // consumer wants to restart reading from the file.
    ResetBodyPipe();

    std::move(callback).Run(net::OK, upload_string_.length());
    upload_body_pipe_ = std::move(pipe);
    handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
        base::SequencedTaskRunner::GetCurrentDefault());
    handle_watcher_->Watch(
        upload_body_pipe_.get(),
        // Don't bother watching for close - rely on read pipes for errors.
        MOJO_HANDLE_SIGNAL_WRITABLE, MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(&StringUploadDataPipeGetter::MojoReadyCallback,
                            base::Unretained(this)));
    WriteData();
  }

  void Clone(mojo::PendingReceiver<mojom::DataPipeGetter> receiver) override {
    receiver_set_.Add(this, std::move(receiver));
  }

  void MojoReadyCallback(MojoResult result,
                         const mojo::HandleSignalsState& state) {
    TRACE_EVENT("toplevel", "SimpleURLLoader_BodyReader mojo callback",
                [&](perfetto::EventContext& ctx) {
                  ctx.event()->set_source_location_iid(
                      base::trace_event::InternedSourceLocation::Get(
                          &ctx, url_loader_created_from_));
                });
    WriteData();
  }

  void WriteData() {
    DCHECK_LE(write_position_, upload_string_.length());

    while (true) {
      base::span<const uint8_t> bytes = base::as_byte_span(upload_string_);
      bytes = bytes.subspan(write_position_);
      bytes = bytes.first(std::min(bytes.size(), size_t{32 * 1024}));
      if (bytes.empty()) {
        // Upload is done. Close the upload body pipe and wait for another call
        // to Read().
        ResetBodyPipe();
        return;
      }

      size_t actually_written_bytes;
      MojoResult result = upload_body_pipe_->WriteData(
          bytes, MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        handle_watcher_->ArmOrNotify();
        return;
      }

      if (result != MOJO_RESULT_OK) {
        // Ignore the pipe being closed - the upload may still be retried with
        // another call to Read.
        ResetBodyPipe();
        return;
      }

      write_position_ += actually_written_bytes;
      DCHECK_LE(write_position_, upload_string_.length());
    }
  }

  // Closes the body pipe, and resets the position the class is writing from.
  // Should be called either when a new receiver is created, or a new read
  // through the file is started.
  void ResetBodyPipe() {
    handle_watcher_.reset();
    upload_body_pipe_.reset();
    write_position_ = 0;
  }

  mojo::ReceiverSet<mojom::DataPipeGetter> receiver_set_;

  mojo::ScopedDataPipeProducerHandle upload_body_pipe_;
  // Must be below |write_pipe_|, so it's deleted first.
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;
  size_t write_position_ = 0;

  const std::string upload_string_;
  const base::Location url_loader_created_from_;
};

class BodyHandler;

class SimpleURLLoaderImpl : public SimpleURLLoader,
                            public mojom::URLLoaderClient {
 public:
  SimpleURLLoaderImpl(std::unique_ptr<ResourceRequest> resource_request,
                      const net::NetworkTrafficAnnotationTag& annotation_tag,
                      const base::Location& created_from);

  SimpleURLLoaderImpl(const SimpleURLLoaderImpl&) = delete;
  SimpleURLLoaderImpl& operator=(const SimpleURLLoaderImpl&) = delete;

  ~SimpleURLLoaderImpl() override;

  // SimpleURLLoader implementation.
  void DownloadToString(mojom::URLLoaderFactory* url_loader_factory,
                        BodyAsStringCallbackDeprecated body_as_string_callback,
                        size_t max_body_size) override;
  void DownloadToString(mojom::URLLoaderFactory* url_loader_factory,
                        BodyAsStringCallback body_as_string_callback,
                        size_t max_body_size) override;
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      mojom::URLLoaderFactory* url_loader_factory,
      BodyAsStringCallbackDeprecated body_as_string_callback) override;
  void DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      mojom::URLLoaderFactory* url_loader_factory,
      BodyAsStringCallback body_as_string_callback) override;
  void DownloadHeadersOnly(mojom::URLLoaderFactory* url_loader_factory,
                           HeadersOnlyCallback headers_only_callback) override;
  void DownloadToFile(
      mojom::URLLoaderFactory* url_loader_factory,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      const base::FilePath& file_path,
      int64_t max_body_size) override;
  void DownloadToTempFile(
      mojom::URLLoaderFactory* url_loader_factory,
      DownloadToFileCompleteCallback download_to_file_complete_callback,
      int64_t max_body_size) override;
  void DownloadAsStream(
      mojom::URLLoaderFactory* url_loader_factory,
      SimpleURLLoaderStreamConsumer* stream_consumer) override;
  void SetOnRedirectCallback(
      const OnRedirectCallback& on_redirect_callback) override;
  void SetOnResponseStartedCallback(
      OnResponseStartedCallback on_response_started_callback) override;
  void SetOnUploadProgressCallback(
      UploadProgressCallback on_upload_progress_callback) override;
  void SetOnDownloadProgressCallback(
      DownloadProgressCallback on_download_progress_callback) override;
  void SetAllowPartialResults(bool allow_partial_results) override;
  void SetAllowHttpErrorResults(bool allow_http_error_results) override;
  void AttachStringForUpload(std::string_view upload_data,
                             std::string_view upload_content_type) override;
  void AttachStringForUpload(std::string_view upload_data) override;
  void AttachStringForUpload(const char* upload_data,
                             std::string_view upload_content_type) override;
  void AttachStringForUpload(const char* upload_data) override;
  void AttachStringForUpload(std::string&& upload_data,
                             std::string_view upload_content_type) override;
  void AttachStringForUpload(std::string&& upload_data) override;
  void AttachFileForUpload(
      const base::FilePath& upload_file_path,
      const std::string& upload_content_type,
      uint64_t offset = 0,
      uint64_t length = std::numeric_limits<uint64_t>::max()) override;
  void AttachFileForUpload(
      const base::FilePath& upload_file_path,
      uint64_t offset = 0,
      uint64_t length = std::numeric_limits<uint64_t>::max()) override;
  void SetRetryOptions(int max_retries, int retry_mode) override;
  void SetURLLoaderFactoryOptions(uint32_t options) override;
  void SetRequestID(int32_t request_id) override;
  void SetTimeoutDuration(base::TimeDelta timeout_duration) override;

  int NetError() const override;
  const mojom::URLResponseHead* ResponseInfo() const override;
  mojom::URLResponseHeadPtr TakeResponseInfo() override;
  const std::optional<URLLoaderCompletionStatus>& CompletionStatus()
      const override;
  const GURL& GetFinalURL() const override;
  bool LoadedFromCache() const override;
  int64_t GetContentSize() const override;
  int GetNumRetries() const override;

  // Called by BodyHandler when the BodyHandler body handler is done. If |error|
  // is not net::OK, some error occurred reading or consuming the body. If it is
  // net::OK, the pipe was closed and all data received was successfully
  // handled. This could indicate an error, cancellation, or completion. To
  // determine which case this is, the size will also be compared to the size
  // reported in URLLoaderCompletionStatus(), if
  // URLLoaderCompletionStatus indicates a success.
  void OnBodyHandlerDone(net::Error error, int64_t received_body_size);

  // Called by BodyHandler to report download progress.
  void OnBodyHandlerProgress(int64_t downloaded);

  // Posted to report download progress in a non-reentrant way.
  void DispatchDownloadProgress(int64_t downloaded);

  // Finished the request with the provided error code, after freeing Mojo
  // resources. Closes any open pipes, so no URLLoader or BodyHandlers callbacks
  // will be invoked after this is called.
  void FinishWithResult(int net_error);

  const base::Location& created_from() const { return created_from_; }

 private:
  // Per-request state values. This object is re-created for each retry.
  // Separating out the values makes re-initializing them on retry simpler.
  struct RequestState {
    RequestState() = default;
    ~RequestState() = default;

    bool request_completed = false;

    bool body_started = false;
    bool body_completed = false;
    // Final size of the body. Set once the body's Mojo pipe has been closed.
    // Set to kReceivedBodySizeUnknown if we never actually read a body.
    int64_t received_body_size = 0;

    // Set to true when FinishWithResult() is called. Once that happens, the
    // consumer is informed of completion, and both pipes are closed.
    bool finished = false;

    // Result of the request.
    int net_error = net::ERR_IO_PENDING;

    bool loaded_from_cache = false;

    mojom::URLResponseHeadPtr response_info;

    std::optional<URLLoaderCompletionStatus> completion_status;
  };

  // Need two different methods to avoid a double-copy (string copy, then copy
  // to a vector) when need to call UploadData::AppendBytes() for short request
  // bodies.
  void AttachStringForUploadInternal(
      std::string_view upload_data,
      base::optional_ref<std::string_view> upload_content_type);
  void AttachStringForUploadInternal(
      std::string&& upload_data,
      base::optional_ref<std::string_view> upload_content_type);

  void AttachFileForUpload(
      const base::FilePath& upload_file_path,
      const std::string* const upload_content_type,
      uint64_t offset = 0,
      uint64_t length = std::numeric_limits<uint64_t>::max());

  // Prepares internal state to start a request, and then calls StartRequest().
  // Only used for the initial request (Not retries).
  void Start(mojom::URLLoaderFactory* url_loader_factory);

  void OnReadyToStart();

  // Starts a request. Used for both the initial request and retries, if any.
  void StartRequest(mojom::URLLoaderFactory* url_loader_factory);

  // Re-initializes state of |this| and |body_handler_| prior to retrying a
  // request.
  void Retry();

  // mojom::URLLoaderClient implementation;
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         mojom::URLResponseHeadPtr response_head) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override;
  void OnComplete(const URLLoaderCompletionStatus& status) override;

  // Choose the TaskPriority based on |resource_request_|'s net priority.
  // TODO(mmenke): Can something better be done here?
  base::TaskPriority GetTaskPriority() const {
    base::TaskPriority task_priority;
    if (resource_request_->priority >= net::MEDIUM) {
      task_priority = base::TaskPriority::USER_BLOCKING;
    } else if (resource_request_->priority >= net::LOW) {
      task_priority = base::TaskPriority::USER_VISIBLE;
    } else {
      task_priority = base::TaskPriority::BEST_EFFORT;
    }
    return task_priority;
  }

  // Bound to the URLLoaderClient message pipe (|client_receiver_|) via
  // set_disconnect_handler.
  void OnMojoDisconnect();

  // Completes the request by calling FinishWithResult() if OnComplete() was
  // called and either no body pipe was ever received, or the body pipe was
  // closed.
  void MaybeComplete();

  OnRedirectCallback on_redirect_callback_;
  OnResponseStartedCallback on_response_started_callback_;
  UploadProgressCallback on_upload_progress_callback_;
  DownloadProgressCallback on_download_progress_callback_;
  bool allow_partial_results_ = false;
  bool allow_http_error_results_ = false;

  // Information related to retrying.
  int remaining_retries_ = 0;
  int retry_mode_ = RETRY_NEVER;
  uint32_t url_loader_factory_options_ = 0;
  int32_t request_id_ = 0;
  int num_retries_ = 0;

  // The next values contain all the information required to restart the
  // request.

  // Populated in the constructor, and cleared once no longer needed, when no
  // more retries are possible.
  std::unique_ptr<ResourceRequest> resource_request_;
  const net::NetworkTrafficAnnotationTag annotation_tag_;
  const base::Location created_from_;
  // Cloned from the input URLLoaderFactory if it may be needed to follow
  // redirects.
  mojo::Remote<mojom::URLLoaderFactory> url_loader_factory_remote_;
  std::unique_ptr<BodyHandler> body_handler_;

  mojo::Receiver<mojom::URLLoaderClient> client_receiver_{this};
  mojo::Remote<mojom::URLLoader> url_loader_;

  std::unique_ptr<StringUploadDataPipeGetter> string_upload_data_pipe_getter_;

  // Per-request state. Always non-null, but re-created on redirect.
  std::unique_ptr<RequestState> request_state_;

  GURL final_url_;

  // The timer that triggers a timeout when a request takes too long.
  base::OneShotTimer timeout_timer_;
  // How long |timeout_timer_| should wait before timing out a request. A value
  // of zero means do not set a timeout.
  base::TimeDelta timeout_duration_ = base::TimeDelta();

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SimpleURLLoaderImpl> weak_ptr_factory_{this};
};

// Utility class to drive the pipe reading a response body. Can be created on
// one thread and then used to read data on another. A BodyReader may only be
// used once. If a request is retried, a new one must be created.
class BodyReader {
 public:
  class Delegate {
   public:
    Delegate() {}

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // The specified amount of data was read from the pipe. The Delegate should
    // return net::OK to continue reading, or a value indicating an error if the
    // pipe should be closed.  A return value of net::ERR_IO_PENDING means that
    // the BodyReader should stop reading, and not call OnDone(), until its
    // Resume() method is called. Resume() must not be called synchronously.
    //
    // It's safe to delete the BodyReader during this call. If that happens,
    // |data| will still remain valid for the duration of the call, and the
    // returned net::Error will be ignored.
    virtual net::Error OnDataRead(uint32_t length, const char* data) = 0;

    // Called when the pipe is closed by the remote size, the size limit is
    // reached, or OnDataRead returned an error. |error| is net::OK if the
    // pipe was closed, or an error value otherwise. It is safe to delete the
    // BodyReader during this callback.
    virtual void OnDone(net::Error error, int64_t total_bytes) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  BodyReader(Delegate* delegate,
             int64_t max_body_size,
             const base::Location& url_loader_created_from)
      : delegate_(delegate),
        max_body_size_(max_body_size),
        url_loader_created_from_(url_loader_created_from) {
    DCHECK_GE(max_body_size_, 0);
  }

  BodyReader(const BodyReader&) = delete;
  BodyReader& operator=(const BodyReader&) = delete;

  // Makes the reader start reading from |body_data_pipe|. May only be called
  // once. The reader will continuously to try to read from the pipe (without
  // blocking the thread), calling OnDataRead as data is read, until one of the
  // following happens:
  // * The size limit is reached.
  // * OnDataRead returns an error.
  // * The BodyReader is deleted.
  void Start(mojo::ScopedDataPipeConsumerHandle body_data_pipe) {
    DCHECK(!body_data_pipe_.is_valid());
    body_data_pipe_ = std::move(body_data_pipe);
    handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
        base::SequencedTaskRunner::GetCurrentDefault());
    handle_watcher_->Watch(
        body_data_pipe_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_WATCH_CONDITION_SATISFIED,
        base::BindRepeating(&BodyReader::MojoReadyCallback,
                            base::Unretained(this)));
    ReadData();
  }

  void Resume() { ReadData(); }

  int64_t total_bytes_read() { return total_bytes_read_; }

 private:
  void MojoReadyCallback(MojoResult result,
                         const mojo::HandleSignalsState& state) {
    TRACE_EVENT("toplevel", "SimpleURLLoader_BodyReader mojo callback",
                [&](perfetto::EventContext& ctx) {
                  ctx.event()->set_source_location_iid(
                      base::trace_event::InternedSourceLocation::Get(
                          &ctx, url_loader_created_from_));
                });
    // Shouldn't be watching the pipe when there's a pending error.
    DCHECK_EQ(net::OK, pending_error_);

    ReadData();
  }

  // Reads as much data as possible from |body_data_pipe_|, copying it to
  // |body_|. Arms |handle_watcher_| when data is not currently available.
  void ReadData() {
    while (true) {
      if (pending_error_) {
        ClosePipe();
        // This call may delete the BodyReader.
        delegate_->OnDone(pending_error_, total_bytes_read_);
        return;
      }

      base::span<const uint8_t> body;
      MojoResult result =
          body_data_pipe_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, body);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        handle_watcher_->ArmOrNotify();
        return;
      }

      // If the pipe was closed, unclear if it was an error or success. Notify
      // the consumer of how much data was received.
      if (result != MOJO_RESULT_OK) {
        // The only error other than MOJO_RESULT_SHOULD_WAIT this should fail
        // with is MOJO_RESULT_FAILED_PRECONDITION, in the case the pipe was
        // closed.
        DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, result);
        ClosePipe();

        // This call may delete the BodyReader.
        delegate_->OnDone(net::OK, total_bytes_read_);
        return;
      }

      // Check size against the limit.
      size_t copy_size = body.size();
      size_t limit =
          base::saturated_cast<size_t>(max_body_size_ - total_bytes_read_);
      if (copy_size > limit) {
        copy_size = limit;
      }

      total_bytes_read_ += copy_size;

      if (copy_size < body.size()) {
        pending_error_ = net::ERR_INSUFFICIENT_RESOURCES;
      }

      // Need a weak pointer to |this| to detect deletion.
      base::WeakPtr<BodyReader> weak_this =
          this->weak_ptr_factory_.GetWeakPtr();
      // Need to keep the data pipe alive if |this| is deleted, to keep
      // |body_data| alive. Also unclear if it's safe to delete while a read is
      // in progress.
      mojo::ScopedDataPipeConsumerHandle body_data_pipe =
          std::move(body_data_pipe_);

      // TODO(mmenke): Remove this once https://crbug.com/875253 is understood
      // and fixed.
      std::string_view chars = base::as_string_view(body);
      int total_bytes_read = total_bytes_read_;
      int max_body_size = max_body_size_;
      base::debug::Alias(&body);
      base::debug::Alias(&max_body_size);
      base::debug::Alias(&total_bytes_read);
      base::debug::Alias(&copy_size);
      // This is just to make sure the first byte of body_data is accessible.
      char first_read_byte = chars[0];
      base::debug::Alias(&first_read_byte);

      // This call may delete the BodyReader.
      net::Error error = delegate_->OnDataRead(copy_size, chars.data());
      body_data_pipe->EndReadData(chars.size());
      if (!weak_this) {
        // This object was deleted, so nothing else to do.
        return;
      }

      body_data_pipe_ = std::move(body_data_pipe);

      // Wait for Resume() on net::ERR_IO_PENDING.
      if (error == net::ERR_IO_PENDING)
        return;

      if (error != net::OK)
        pending_error_ = error;
    }
  }

  // Frees Mojo resources and prevents any more Mojo messages from arriving.
  void ClosePipe() {
    handle_watcher_.reset();
    body_data_pipe_.reset();
  }

  mojo::ScopedDataPipeConsumerHandle body_data_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> handle_watcher_;

  const raw_ptr<Delegate> delegate_;

  const int64_t max_body_size_;
  int64_t total_bytes_read_ = 0;

  const base::Location url_loader_created_from_;

  // Set to an error code when Delegate::OnDataRead() returns ERR_IO_PENDING,
  // and there was a pending error from the BodyReader itself (Generally, length
  // limit exceeded). When this happens, the error will be passed to the
  // Delegate only after Resume() is called.
  net::Error pending_error_ = net::OK;

  base::WeakPtrFactory<BodyReader> weak_ptr_factory_{this};
};

// Class to drive the pipe for reading the body, handle the results of the body
// read as appropriate, and invoke the consumer's callback to notify it of
// request completion. Implementations typically use a BodyReader to to manage
// reads from the body data pipe.
class BodyHandler {
 public:
  // A raw pointer is safe, since |simple_url_loader| owns the BodyHandler.
  BodyHandler(SimpleURLLoaderImpl* simple_url_loader,
              bool want_download_progress)
      : simple_url_loader_(simple_url_loader),
        want_download_progress_(want_download_progress) {}

  BodyHandler(const BodyHandler&) = delete;
  BodyHandler& operator=(const BodyHandler&) = delete;

  virtual ~BodyHandler() = default;

  // Called by SimpleURLLoader if no data pipe was received from the URLLoader.
  // Returns whether or not a data pipe is required. In most cases a data pipe
  // is required for a successful response, but if we don't need the body then
  // this can return false. If there is no data pipe and this method returns
  // false, OnStartLoadingResponseBody() will not be called.
  virtual bool RequiresBodyDataPipe() { return true; }

  // Called by SimpleURLLoader with the data pipe received from the URLLoader.
  // The BodyHandler is responsible for reading from it and monitoring it for
  // closure. Should call SimpleURLLoaderImpl::OnBodyHandlerDone(), once either
  // when the body pipe is closed or when an error occurs, like a write to a
  // file fails.
  virtual void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) = 0;

  // Called by SimpleURLLoader. Notifies the SimpleURLLoader's consumer that the
  // request has completed, either successfully or with an error. May be invoked
  // before OnStartLoadingResponseBody(), or before the BodyHandler has notified
  // SimplerURLLoader of completion or an error. Once this is called, the
  // BodyHandler must not invoke any of SimpleURLLoaderImpl's callbacks. If
  // |destroy_results| is true, any received data should be destroyed instead of
  // being sent to the consumer.
  virtual void NotifyConsumerOfCompletion(bool destroy_results) = 0;

  // Called before retrying a request. Only called either before receiving a
  // body pipe, or after the body pipe has been closed, so there should be no
  // pending callbacks when invoked. |retry_callback| should be invoked when
  // the BodyHandler is ready for the request to be retried. Callback may be
  // invoked synchronously.
  virtual void PrepareToRetry(base::OnceClosure retry_callback) = 0;

 protected:
  SimpleURLLoaderImpl* simple_url_loader() { return simple_url_loader_; }

  void ReportProgress(int64_t total_downloaded) {
    if (!want_download_progress_)
      return;
    simple_url_loader_->OnBodyHandlerProgress(total_downloaded);
  }

 private:
  const raw_ptr<SimpleURLLoaderImpl> simple_url_loader_;
  bool const want_download_progress_;
};

// BodyHandler implementation for consuming the response as a string.
class SaveToStringBodyHandler : public BodyHandler,
                                public BodyReader::Delegate {
 public:
  SaveToStringBodyHandler(
      SimpleURLLoaderImpl* simple_url_loader,
      bool want_download_progress,
      SimpleURLLoader::BodyAsStringCallbackDeprecated body_as_string_callback,
      int64_t max_body_size)
      : BodyHandler(simple_url_loader, want_download_progress),
        max_body_size_(max_body_size),
        body_as_string_callback_(std::move(body_as_string_callback)),
        url_loader_created_from_(simple_url_loader->created_from()) {}

  SaveToStringBodyHandler(const SaveToStringBodyHandler&) = delete;
  SaveToStringBodyHandler& operator=(const SaveToStringBodyHandler&) = delete;

  ~SaveToStringBodyHandler() override = default;

  // BodyHandler implementation:

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override {
    DCHECK(!body_);
    DCHECK(!body_reader_);

    body_ = std::make_unique<std::string>();
    body_reader_ = std::make_unique<BodyReader>(this, max_body_size_,
                                                url_loader_created_from_);
    body_reader_->Start(std::move(body_data_pipe));
  }

  void NotifyConsumerOfCompletion(bool destroy_results) override {
    body_reader_.reset();
    if (destroy_results)
      body_.reset();

    std::move(body_as_string_callback_).Run(std::move(body_));
  }

  void PrepareToRetry(base::OnceClosure retry_callback) override {
    body_.reset();
    body_reader_.reset();
    std::move(retry_callback).Run();
  }

 private:
  // BodyReader::Delegate implementation.

  net::Error OnDataRead(uint32_t length, const char* data) override {
    // TODO(mmenke): Remove this once https://crbug.com/875253 is understood and
    // fixed.
    std::string* body = body_.get();
    base::debug::Alias(&body);

    body_->append(data, length);
    ReportProgress(body_reader_->total_bytes_read());
    return net::OK;
  }

  void OnDone(net::Error error, int64_t total_bytes) override {
    DCHECK_EQ(body_->size(), static_cast<size_t>(total_bytes));
    simple_url_loader()->OnBodyHandlerDone(error, total_bytes);
  }

  const int64_t max_body_size_;

  std::unique_ptr<std::string> body_;
  SimpleURLLoader::BodyAsStringCallbackDeprecated body_as_string_callback_;

  const base::Location url_loader_created_from_;

  std::unique_ptr<BodyReader> body_reader_;
};

// BodyHandler that discards the response body.
class HeadersOnlyBodyHandler : public BodyHandler, public BodyReader::Delegate {
 public:
  HeadersOnlyBodyHandler(
      SimpleURLLoaderImpl* simple_url_loader,
      SimpleURLLoader::HeadersOnlyCallback headers_only_callback)
      : BodyHandler(simple_url_loader, false /* no download progress */),
        headers_only_callback_(std::move(headers_only_callback)),
        url_loader_created_from_(simple_url_loader->created_from()) {}

  HeadersOnlyBodyHandler(const HeadersOnlyBodyHandler&) = delete;
  HeadersOnlyBodyHandler& operator=(const HeadersOnlyBodyHandler&) = delete;

  ~HeadersOnlyBodyHandler() override = default;

  // BodyHandler implementation
  bool RequiresBodyDataPipe() override { return false; }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override {
    // TODO(crbug.com/41406327): The request can be completed at this point
    // however that requires more changes to SimpleURLLoader as OnComplete()
    // will not have been called yet.
    DCHECK(!body_reader_);
    body_reader_ = std::make_unique<BodyReader>(
        this, std::numeric_limits<int64_t>::max(), url_loader_created_from_);
    body_reader_->Start(std::move(body_data_pipe));
  }

  void NotifyConsumerOfCompletion(bool destroy_results) override {
    body_reader_.reset();
    std::move(headers_only_callback_)
        .Run(simple_url_loader()->ResponseInfo()
                 ? simple_url_loader()->ResponseInfo()->headers
                 : nullptr);
  }

  void PrepareToRetry(base::OnceClosure retry_callback) override {
    body_reader_.reset();
    std::move(retry_callback).Run();
  }

 private:
  // BodyReader::Delegate implementation
  net::Error OnDataRead(uint32_t length, const char* data) override {
    return net::OK;
  }

  void OnDone(net::Error error, int64_t total_bytes) override {
    simple_url_loader()->OnBodyHandlerDone(error, total_bytes);
  }

  SimpleURLLoader::HeadersOnlyCallback headers_only_callback_;
  std::unique_ptr<BodyReader> body_reader_;
  const base::Location url_loader_created_from_;
};

// BodyHandler implementation for saving the response to a file
class SaveToFileBodyHandler : public BodyHandler {
 public:
  // |net_priority| is the priority from the ResourceRequest, and is used to
  // determine the TaskPriority of the sequence used to read from the response
  // body and write to the file. If |create_temp_file| is true, a temp file is
  // created instead of using |path|.
  SaveToFileBodyHandler(SimpleURLLoaderImpl* simple_url_loader,
                        bool want_download_progress,
                        SimpleURLLoader::DownloadToFileCompleteCallback
                            download_to_file_complete_callback,
                        const base::FilePath& path,
                        bool create_temp_file,
                        uint64_t max_body_size,
                        base::TaskPriority task_priority)
      : BodyHandler(simple_url_loader, want_download_progress),
        download_to_file_complete_callback_(
            std::move(download_to_file_complete_callback)) {
    DCHECK(create_temp_file || !path.empty());

    // Can only do this after initializing the WeakPtrFactory.
    file_writer_ = std::make_unique<FileWriter>(
        path, create_temp_file, max_body_size, task_priority,
        want_download_progress
            ? base::BindRepeating(&SaveToFileBodyHandler::ReportProgress,
                                  weak_ptr_factory_.GetWeakPtr())
            : base::RepeatingCallback<void(int64_t)>(),
        simple_url_loader->created_from());
  }

  SaveToFileBodyHandler(const SaveToFileBodyHandler&) = delete;
  SaveToFileBodyHandler& operator=(const SaveToFileBodyHandler&) = delete;

  ~SaveToFileBodyHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (file_writer_) {
      // |file_writer_| is only non-null at this point if any downloaded file
      // wasn't passed to the consumer. Destroy any partially downloaded file.
      file_writer_->DeleteFile(base::OnceClosure());
      FileWriter::Destroy(std::move(file_writer_));
    }
  }

  // BodyHandler implementation:
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    file_writer_->StartWriting(std::move(body_data_pipe),
                               base::BindOnce(&SaveToFileBodyHandler::OnDone,
                                              weak_ptr_factory_.GetWeakPtr()));
  }

  void NotifyConsumerOfCompletion(bool destroy_results) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!download_to_file_complete_callback_.is_null());

    if (destroy_results) {
      // Prevent the FileWriter from calling OnDone().
      weak_ptr_factory_.InvalidateWeakPtrs();

      // To avoid any issues if the consumer tries to re-download a file to the
      // same location, don't invoke the callback until any partially downloaded
      // file has been destroyed.
      file_writer_->DeleteFile(
          base::BindOnce(&SaveToFileBodyHandler::InvokeCallbackAsynchronously,
                         weak_ptr_factory_.GetWeakPtr()));
      FileWriter::Destroy(std::move(file_writer_));
      return;
    }

    // Destroy the |file_writer_|, so the file won't be destroyed in |this|'s
    // destructor.
    FileWriter::Destroy(std::move(file_writer_));

    std::move(download_to_file_complete_callback_).Run(std::move(path_));
  }

  void PrepareToRetry(base::OnceClosure retry_callback) override {
    // |file_writer_| is only destroyed when notifying the consumer of
    // completion and in the destructor. After either of those happens, a
    // request should not be retried.
    DCHECK(file_writer_);

    // Delete file and wait for it to be destroyed, so if the retry fails
    // before trying to create a new file, the consumer will still only be
    // notified of completion after the file is destroyed.
    file_writer_->DeleteFile(std::move(retry_callback));
  }

 private:
  // Class to read from a mojo::ScopedDataPipeConsumerHandle and write the
  // contents to a file. Does all reading and writing on a separate file
  // SequencedTaskRunner. All public methods except the destructor are called on
  // BodyHandler's TaskRunner.  All private methods and the destructor are run
  // on the file TaskRunner.
  //
  // FileWriter is owned by the SaveToFileBodyHandler and destroyed by a task
  // moving its unique_ptr to the |file_writer_task_runner_|. As a result, tasks
  // posted to |file_writer_task_runner_| can always use base::Unretained. Tasks
  // posted the other way, however, require the SaveToFileBodyHandler to use
  // WeakPtrs, since the SaveToFileBodyHandler can be destroyed at any time.
  //
  // When a request is retried, the FileWriter deletes any partially downloaded
  // file, and is then reused.
  class FileWriter : public BodyReader::Delegate {
   public:
    using OnDoneCallback = base::OnceCallback<void(net::Error error,
                                                   int64_t total_bytes,
                                                   const base::FilePath& path)>;

    FileWriter(const base::FilePath& path,
               bool create_temp_file,
               int64_t max_body_size,
               base::TaskPriority priority,
               base::RepeatingCallback<void(int64_t)> progress_callback,
               const base::Location& url_loader_created_from)
        : body_handler_task_runner_(
              base::SequencedTaskRunner::GetCurrentDefault()),
          file_writer_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), priority,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
          path_(path),
          create_temp_file_(create_temp_file),
          max_body_size_(max_body_size),
          progress_callback_(progress_callback),
          url_loader_created_from_(url_loader_created_from) {
      DCHECK(body_handler_task_runner_->RunsTasksInCurrentSequence());
      DCHECK(create_temp_file_ || !path_.empty());
    }

    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;

    // Starts reading from |body_data_pipe| and writing to the file.
    void StartWriting(mojo::ScopedDataPipeConsumerHandle body_data_pipe,
                      OnDoneCallback on_done_callback) {
      DCHECK(body_handler_task_runner_->RunsTasksInCurrentSequence());
      file_writer_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&FileWriter::StartWritingOnFileSequence,
                         base::Unretained(this), std::move(body_data_pipe),
                         std::move(on_done_callback)));
    }

    // Deletes any partially downloaded file, and closes the body pipe, if open.
    // Must be called if SaveToFileBodyHandler's OnDone() method is never
    // invoked, to avoid keeping around partial downloads that were never passed
    // to the consumer.
    //
    // If |on_file_deleted_closure| is non-null, it will be invoked on the
    // caller's task runner once the file has been deleted.
    void DeleteFile(base::OnceClosure on_file_deleted_closure) {
      DCHECK(body_handler_task_runner_->RunsTasksInCurrentSequence());
      file_writer_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&FileWriter::DeleteFileOnFileSequence,
                                    base::Unretained(this),
                                    std::move(on_file_deleted_closure)));
    }

    // Destroys the FileWriter on the file TaskRunner.
    static void Destroy(std::unique_ptr<FileWriter> file_writer) {
      DCHECK(
          file_writer->body_handler_task_runner_->RunsTasksInCurrentSequence());

      // Have to stash this pointer before posting a task, since |file_writer|
      // is bound to the callback that's posted to the TaskRunner.
      base::SequencedTaskRunner* task_runner =
          file_writer->file_writer_task_runner_.get();
      task_runner->DeleteSoon(FROM_HERE, std::move(file_writer));
    }

    // Destructor is only public so the consumer can keep it in a unique_ptr.
    // Class must be destroyed by using Destroy().
    ~FileWriter() override {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
    }

   private:
    void StartWritingOnFileSequence(
        mojo::ScopedDataPipeConsumerHandle body_data_pipe,
        OnDoneCallback on_done_callback) {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      DCHECK(!file_.IsValid());
      DCHECK(!body_reader_);

      bool have_path = !create_temp_file_;
      if (!have_path) {
        DCHECK(create_temp_file_);
        have_path = base::CreateTemporaryFile(&path_);
        // CreateTemporaryFile() creates an empty file.
        if (have_path)
          owns_file_ = true;
      }

      if (have_path) {
        // Try to initialize |file_|, creating the file if needed.
        file_.Initialize(
            path_, base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
      }

      // If CreateTemporaryFile() or File::Initialize() failed, report failure.
      if (!file_.IsValid()) {
        body_handler_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(std::move(on_done_callback),
                                      net::MapSystemError(
                                          logging::GetLastSystemErrorCode()),
                                      0, base::FilePath()));
        return;
      }

      on_done_callback_ = std::move(on_done_callback);
      owns_file_ = true;
      body_reader_ = std::make_unique<BodyReader>(this, max_body_size_,
                                                  url_loader_created_from_);
      body_reader_->Start(std::move(body_data_pipe));
    }

    // BodyReader::Delegate implementation:
    net::Error OnDataRead(uint32_t length, const char* data) override {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      while (length > 0) {
        int written = file_.WriteAtCurrentPos(
            data, std::min(length, static_cast<uint32_t>(
                                       std::numeric_limits<int>::max())));
        if (written < 0)
          return net::MapSystemError(logging::GetLastSystemErrorCode());
        length -= written;
        data += written;
      }

      if (progress_callback_) {
        body_handler_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(progress_callback_,
                                      body_reader_->total_bytes_read()));
      }

      return net::OK;
    }

    void OnDone(net::Error error, int64_t total_bytes) override {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());
      // This should only be called if the file was successfully created.
      DCHECK(file_.IsValid());

      // Close the file so that there's no ownership contention when the
      // consumer uses it.
      file_.Close();
      body_reader_.reset();

      body_handler_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(on_done_callback_), error,
                                    total_bytes, path_));
    }

    void DeleteFileOnFileSequence(base::OnceClosure on_file_deleted_closure) {
      DCHECK(file_writer_task_runner_->RunsTasksInCurrentSequence());

      if (owns_file_) {
        // Close the file before deleting it, if it's still open.
        file_.Close();

        // Close the body pipe.
        body_reader_.reset();

        // May as well clean this up, too.
        on_done_callback_.Reset();

        DCHECK(!path_.empty());
        base::DeleteFile(path_);

        owns_file_ = false;
      }

      if (on_file_deleted_closure) {
        body_handler_task_runner_->PostTask(FROM_HERE,
                                            std::move(on_file_deleted_closure));
      }
    }

    // These are set on construction and accessed on both task runners.
    const scoped_refptr<base::SequencedTaskRunner> body_handler_task_runner_;
    const scoped_refptr<base::SequencedTaskRunner> file_writer_task_runner_;

    // After construction, all other values are only read and written on the
    // |file_writer_task_runner_|.

    base::FilePath path_;
    const bool create_temp_file_;
    const int64_t max_body_size_;

    // If not is_null(), should be invoked on |body_handler_task_runner_| to
    // report progress.
    base::RepeatingCallback<void(int64_t)> progress_callback_;

    const base::Location url_loader_created_from_;

    // File being downloaded to. Created just before reading from the data pipe.
    base::File file_;

    OnDoneCallback on_done_callback_;

    std::unique_ptr<BodyReader> body_reader_;

    // True if a file was successfully created. Set to false when the file is
    // destroyed.
    bool owns_file_ = false;
  };

  // Called by FileWriter::Destroy after deleting a partially downloaded file.
  void InvokeCallbackAsynchronously() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(download_to_file_complete_callback_).Run(base::FilePath());
  }

  void OnDone(net::Error error,
              int64_t total_bytes,
              const base::FilePath& path) {
    path_ = path;
    simple_url_loader()->OnBodyHandlerDone(error, total_bytes);
  }

  // Path of the file. Set in OnDone().
  base::FilePath path_;

  SimpleURLLoader::DownloadToFileCompleteCallback
      download_to_file_complete_callback_;

  std::unique_ptr<FileWriter> file_writer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SaveToFileBodyHandler> weak_ptr_factory_{this};
};

// Class to handle streaming data to the consumer as it arrives
class DownloadAsStreamBodyHandler : public BodyHandler,
                                    public BodyReader::Delegate {
 public:
  DownloadAsStreamBodyHandler(SimpleURLLoaderImpl* simple_url_loader,
                              bool want_download_progress,
                              SimpleURLLoaderStreamConsumer* stream_consumer)
      : BodyHandler(simple_url_loader, want_download_progress),
        stream_consumer_(stream_consumer),
        url_loader_created_from_(simple_url_loader->created_from()) {}

  DownloadAsStreamBodyHandler(const DownloadAsStreamBodyHandler&) = delete;
  DownloadAsStreamBodyHandler& operator=(const DownloadAsStreamBodyHandler&) =
      delete;

  ~DownloadAsStreamBodyHandler() override = default;

  // BodyHandler implementation:

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body_data_pipe) override {
    DCHECK(!body_reader_);

    body_reader_ = std::make_unique<BodyReader>(
        this, std::numeric_limits<int64_t>::max(), url_loader_created_from_);
    body_reader_->Start(std::move(body_data_pipe));
  }

  void NotifyConsumerOfCompletion(bool destroy_results) override {
    body_reader_.reset();
    stream_consumer_.ExtractAsDangling()->OnComplete(
        simple_url_loader()->NetError() == net::OK);
  }

  void PrepareToRetry(base::OnceClosure retry_callback) override {
    body_reader_.reset();
    stream_consumer_->OnRetry(std::move(retry_callback));
  }

 private:
  // BodyReader::Delegate implementation.

  net::Error OnDataRead(uint32_t length, const char* data) override {
    in_recursive_call_ = true;
    base::WeakPtr<DownloadAsStreamBodyHandler> weak_this(
        weak_ptr_factory_.GetWeakPtr());
    stream_consumer_->OnDataReceived(
        std::string_view(data, length),
        base::BindOnce(&DownloadAsStreamBodyHandler::Resume,
                       weak_ptr_factory_.GetWeakPtr()));
    // Protect against deletion.
    if (weak_this) {
      // ReportProgress can't trigger deletion itself since it doesn't invoke
      // outside code synchronously.
      ReportProgress(body_reader_->total_bytes_read());
      in_recursive_call_ = false;
    }
    return net::ERR_IO_PENDING;
  }

  void OnDone(net::Error error, int64_t total_bytes) override {
    simple_url_loader()->OnBodyHandlerDone(error, total_bytes);
  }

  void Resume() {
    // Can't call DownloadAsStreamBodyHandler::Resume() immediately when called
    // recursively from OnDataRead.
    if (in_recursive_call_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&DownloadAsStreamBodyHandler::Resume,
                                    weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    if (!body_reader_) {
      // If Resume was delayed, body_reader_ could have been deleted.
      return;
    }
    body_reader_->Resume();
  }

  raw_ptr<SimpleURLLoaderStreamConsumer> stream_consumer_;

  const base::Location url_loader_created_from_;

  std::unique_ptr<BodyReader> body_reader_;

  bool in_recursive_call_ = false;

  base::WeakPtrFactory<DownloadAsStreamBodyHandler> weak_ptr_factory_{this};
};

SimpleURLLoaderImpl::SimpleURLLoaderImpl(
    std::unique_ptr<ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const base::Location& created_from)
    : resource_request_(std::move(resource_request)),
      annotation_tag_(annotation_tag),
      created_from_(created_from),
      request_state_(std::make_unique<RequestState>()),
      timeout_timer_(timeout_tick_clock_) {
  // Allow creation and use on different threads.
  DETACH_FROM_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  if (resource_request_->request_body) {
    for (const DataElement& element :
         *resource_request_->request_body->elements()) {
      // Files should be attached with AttachFileForUpload, so that (Once
      // supported) they can be opened in the current process.

      // Bytes should be attached with AttachStringForUpload to allow
      // streaming of large byte buffers to the network process when uploading.
      DCHECK(element.type() != mojom::DataElementDataView::Tag::kFile &&
             element.type() != mojom::DataElementDataView::Tag::kBytes);
    }
  }
#endif  // DCHECK_IS_ON()
}

SimpleURLLoaderImpl::~SimpleURLLoaderImpl() {}

void SimpleURLLoaderImpl::DownloadToString(
    mojom::URLLoaderFactory* url_loader_factory,
    BodyAsStringCallbackDeprecated body_as_string_callback,
    size_t max_body_size) {
  DCHECK_LE(max_body_size, kMaxBoundedStringDownloadSize);
  body_handler_ = std::make_unique<SaveToStringBodyHandler>(
      this, !on_download_progress_callback_.is_null(),
      std::move(body_as_string_callback), max_body_size);
  Start(url_loader_factory);
}

void SimpleURLLoaderImpl::DownloadToString(
    mojom::URLLoaderFactory* url_loader_factory,
    BodyAsStringCallback body_as_string_callback,
    size_t max_body_size) {
  DownloadToString(url_loader_factory,
                   base::BindOnce(GetFromUniquePtrToOptional,
                                  std::move(body_as_string_callback)),
                   max_body_size);
}

void SimpleURLLoaderImpl::DownloadToStringOfUnboundedSizeUntilCrashAndDie(
    mojom::URLLoaderFactory* url_loader_factory,
    BodyAsStringCallbackDeprecated body_as_string_callback) {
  body_handler_ = std::make_unique<SaveToStringBodyHandler>(
      this, !on_download_progress_callback_.is_null(),
      std::move(body_as_string_callback),
      // int64_t because URLLoaderCompletionStatus::decoded_body_length
      // is an int64_t, not a size_t.
      std::numeric_limits<int64_t>::max());
  Start(url_loader_factory);
}

void SimpleURLLoaderImpl::DownloadToStringOfUnboundedSizeUntilCrashAndDie(
    mojom::URLLoaderFactory* url_loader_factory,
    BodyAsStringCallback body_as_string_callback) {
  DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, base::BindOnce(GetFromUniquePtrToOptional,
                                         std::move(body_as_string_callback)));
}

void SimpleURLLoaderImpl::DownloadHeadersOnly(
    mojom::URLLoaderFactory* url_loader_factory,
    HeadersOnlyCallback headers_only_callback) {
  on_download_progress_callback_.Reset();
  body_handler_ = std::make_unique<HeadersOnlyBodyHandler>(
      this, std::move(headers_only_callback));
  if (base::FeatureList::IsEnabled(
          kSimpleURLLoaderUseReadAndDiscardBodyOption)) {
    url_loader_factory_options_ |= mojom::kURLLoadOptionReadAndDiscardBody;
  }
  Start(url_loader_factory);
}

void SimpleURLLoaderImpl::DownloadToFile(
    mojom::URLLoaderFactory* url_loader_factory,
    DownloadToFileCompleteCallback download_to_file_complete_callback,
    const base::FilePath& file_path,
    int64_t max_body_size) {
  DCHECK(!file_path.empty());
  body_handler_ = std::make_unique<SaveToFileBodyHandler>(
      this, !on_download_progress_callback_.is_null(),
      std::move(download_to_file_complete_callback), file_path,
      false /* create_temp_file */, max_body_size, GetTaskPriority());
  Start(url_loader_factory);
}

void SimpleURLLoaderImpl::DownloadToTempFile(
    mojom::URLLoaderFactory* url_loader_factory,
    DownloadToFileCompleteCallback download_to_file_complete_callback,
    int64_t max_body_size) {
  body_handler_ = std::make_unique<SaveToFileBodyHandler>(
      this, !on_download_progress_callback_.is_null(),
      std::move(download_to_file_complete_callback), base::FilePath(),
      true /* create_temp_file */, max_body_size, GetTaskPriority());
  Start(url_loader_factory);
}

void SimpleURLLoaderImpl::DownloadAsStream(
    mojom::URLLoaderFactory* url_loader_factory,
    SimpleURLLoaderStreamConsumer* stream_consumer) {
  body_handler_ = std::make_unique<DownloadAsStreamBodyHandler>(
      this, !on_download_progress_callback_.is_null(), stream_consumer);
  Start(url_loader_factory);
}

void SimpleURLLoaderImpl::SetOnRedirectCallback(
    const OnRedirectCallback& on_redirect_callback) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);

  on_redirect_callback_ = on_redirect_callback;
}

void SimpleURLLoaderImpl::SetOnResponseStartedCallback(
    OnResponseStartedCallback on_response_started_callback) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);

  on_response_started_callback_ = std::move(on_response_started_callback);
  DCHECK(on_response_started_callback_);
}

void SimpleURLLoaderImpl::SetOnUploadProgressCallback(
    UploadProgressCallback on_upload_progress_callback) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);

  on_upload_progress_callback_ = std::move(on_upload_progress_callback);
  DCHECK(on_upload_progress_callback_);
}

void SimpleURLLoaderImpl::SetOnDownloadProgressCallback(
    DownloadProgressCallback on_download_progress_callback) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);

  on_download_progress_callback_ = on_download_progress_callback;
  DCHECK(on_download_progress_callback_);
}

void SimpleURLLoaderImpl::SetAllowPartialResults(bool allow_partial_results) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);
  allow_partial_results_ = allow_partial_results;
}

void SimpleURLLoaderImpl::SetAllowHttpErrorResults(
    bool allow_http_error_results) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);
  allow_http_error_results_ = allow_http_error_results;
}

void SimpleURLLoaderImpl::AttachStringForUploadInternal(
    std::string_view upload_data,
    base::optional_ref<std::string_view> upload_content_type) {
  // Currently only allow a single string to be attached.
  DCHECK(!resource_request_->request_body);
  DCHECK(resource_request_->method != net::HttpRequestHeaders::kGetMethod &&
         resource_request_->method != net::HttpRequestHeaders::kHeadMethod);

  resource_request_->request_body = new ResourceRequestBody();

  if (upload_data.length() <= kMaxUploadStringSizeToCopy) {
    int copy_length = base::checked_cast<int>(upload_data.length());
    resource_request_->request_body->AppendBytes(upload_data.data(),
                                                 copy_length);
  } else {
    // Don't attach the upload body here.  A new pipe will need to be created
    // each time the request is tried.
    string_upload_data_pipe_getter_ =
        std::make_unique<StringUploadDataPipeGetter>(std::string(upload_data),
                                                     created_from_);
  }

  if (upload_content_type) {
    resource_request_->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                         *upload_content_type);
  }
}

void SimpleURLLoaderImpl::AttachStringForUploadInternal(
    std::string&& upload_data,
    base::optional_ref<std::string_view> upload_content_type) {
  // Currently only allow a single string to be attached.
  DCHECK(!resource_request_->request_body);
  DCHECK(resource_request_->method != net::HttpRequestHeaders::kGetMethod &&
         resource_request_->method != net::HttpRequestHeaders::kHeadMethod);

  resource_request_->request_body = new ResourceRequestBody();

  if (upload_data.length() <= kMaxUploadStringSizeToCopy) {
    int copy_length = base::checked_cast<int>(upload_data.length());
    resource_request_->request_body->AppendBytes(upload_data.data(),
                                                 copy_length);
  } else {
    // Don't attach the upload body here.  A new pipe will need to be created
    // each time the request is tried.
    string_upload_data_pipe_getter_ =
        std::make_unique<StringUploadDataPipeGetter>(std::move(upload_data),
                                                     created_from_);
  }

  if (upload_content_type) {
    resource_request_->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                         *upload_content_type);
  }
}
void SimpleURLLoaderImpl::AttachStringForUpload(
    std::string_view upload_data,
    std::string_view upload_content_type) {
  AttachStringForUploadInternal(upload_data, upload_content_type);
}

void SimpleURLLoaderImpl::AttachStringForUpload(std::string_view upload_data) {
  AttachStringForUploadInternal(upload_data, std::nullopt);
}

void SimpleURLLoaderImpl::AttachStringForUpload(
    const char* upload_data,
    std::string_view upload_content_type) {
  AttachStringForUploadInternal(std::string_view(upload_data),
                                upload_content_type);
}

void SimpleURLLoaderImpl::AttachStringForUpload(const char* upload_data) {
  AttachStringForUploadInternal(std::string_view(upload_data), std::nullopt);
}

void SimpleURLLoaderImpl::AttachStringForUpload(
    std::string&& upload_data,
    std::string_view upload_content_type) {
  AttachStringForUploadInternal(std::move(upload_data), upload_content_type);
}

void SimpleURLLoaderImpl::AttachStringForUpload(std::string&& upload_data) {
  AttachStringForUploadInternal(std::move(upload_data), std::nullopt);
}

void SimpleURLLoaderImpl::AttachFileForUpload(
    const base::FilePath& upload_file_path,
    const std::string* const upload_content_type,
    uint64_t offset,
    uint64_t length) {
  DCHECK(!upload_file_path.empty());

  // Currently only allow a single file to be attached.
  DCHECK(!resource_request_->request_body);
  DCHECK(resource_request_->method != net::HttpRequestHeaders::kGetMethod &&
         resource_request_->method != net::HttpRequestHeaders::kHeadMethod);

  // Create an empty body to make DCHECKing that there's no upload body yet
  // simpler.
  resource_request_->request_body = new ResourceRequestBody();
  // TODO(mmenke): Open the file in the current process and append the file
  // handle instead of the file path.
  resource_request_->request_body->AppendFileRange(upload_file_path, offset,
                                                   length, base::Time());

  if (upload_content_type) {
    resource_request_->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                         *upload_content_type);
  }
}

void SimpleURLLoaderImpl::AttachFileForUpload(
    const base::FilePath& upload_file_path,
    const std::string& upload_content_type,
    uint64_t offset,
    uint64_t length) {
  AttachFileForUpload(upload_file_path, &upload_content_type, offset, length);
}

void SimpleURLLoaderImpl::AttachFileForUpload(
    const base::FilePath& upload_file_path,
    uint64_t offset,
    uint64_t length) {
  AttachFileForUpload(upload_file_path, nullptr, offset, length);
}

void SimpleURLLoaderImpl::SetRetryOptions(int max_retries, int retry_mode) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);
  DCHECK_GE(max_retries, 0);
  // Non-zero |max_retries| makes no sense when retries are disabled.
  DCHECK(max_retries > 0 || retry_mode == RETRY_NEVER);

  remaining_retries_ = max_retries;
  retry_mode_ = retry_mode;

#if DCHECK_IS_ON()
  if (max_retries > 0 && resource_request_->request_body) {
    for (const DataElement& element :
         *resource_request_->request_body->elements()) {
      // Data pipes are single-use, so can't retry uploads when there's a data
      // pipe.
      // TODO(mmenke):  Data pipes can be Cloned(), though, so maybe update code
      // to do that?
      DCHECK(element.type() != mojom::DataElementDataView::Tag::kDataPipe);
    }
  }
#endif  // DCHECK_IS_ON()
}

void SimpleURLLoaderImpl::SetURLLoaderFactoryOptions(uint32_t options) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);
  url_loader_factory_options_ = options;
}

void SimpleURLLoaderImpl::SetRequestID(int32_t request_id) {
  // Check if a request has not yet been started.
  DCHECK(!body_handler_);
  request_id_ = request_id;
}

void SimpleURLLoaderImpl::SetTimeoutDuration(base::TimeDelta timeout_duration) {
  DCHECK(!request_state_->body_started);
  DCHECK(timeout_duration >= base::TimeDelta());
  timeout_duration_ = timeout_duration;
}

int SimpleURLLoaderImpl::NetError() const {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  DCHECK_NE(net::ERR_IO_PENDING, request_state_->net_error);
  return request_state_->net_error;
}

const GURL& SimpleURLLoaderImpl::GetFinalURL() const {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  return final_url_;
}

bool SimpleURLLoaderImpl::LoadedFromCache() const {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  return request_state_->loaded_from_cache;
}

int64_t SimpleURLLoaderImpl::GetContentSize() const {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  return request_state_->received_body_size;
}

int SimpleURLLoaderImpl::GetNumRetries() const {
  return num_retries_;
}

const mojom::URLResponseHead* SimpleURLLoaderImpl::ResponseInfo() const {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  return request_state_->response_info.get();
}

mojom::URLResponseHeadPtr SimpleURLLoaderImpl::TakeResponseInfo() {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  return std::move(request_state_->response_info);
}

const std::optional<URLLoaderCompletionStatus>&
SimpleURLLoaderImpl::CompletionStatus() const {
  // Should only be called once the request is complete.
  DCHECK(request_state_->finished);
  return request_state_->completion_status;
}

void SimpleURLLoaderImpl::OnBodyHandlerDone(net::Error error,
                                            int64_t received_body_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(request_state_->body_started);
  DCHECK(!request_state_->body_completed);

  // If there's an error, fail request and report it immediately.
  if (error != net::OK) {
    // Reset the completion status since the contained metrics like encoded body
    // length and net error are not reliable when the body itself was not
    // successfully completed.
    request_state_->completion_status = std::nullopt;
    // When |allow_partial_results_| is true, a valid body|file_path is
    // passed to the completion callback even in the case of failures.
    // For consistency, it makes sense to also hold the actual decompressed
    // body size in case GetContentSize is called.
    if (allow_partial_results_)
      request_state_->received_body_size = received_body_size;
    FinishWithResult(error);
    return;
  }

  // Otherwise, need to wait until the URLRequestClient pipe receives a complete
  // message or is closed, to determine if the entire body was received.
  request_state_->body_completed = true;
  request_state_->received_body_size = received_body_size;
  MaybeComplete();
}

void SimpleURLLoaderImpl::OnBodyHandlerProgress(int64_t progress) {
  if (on_download_progress_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SimpleURLLoaderImpl::DispatchDownloadProgress,
                       weak_ptr_factory_.GetWeakPtr(), progress));
  }
}

void SimpleURLLoaderImpl::DispatchDownloadProgress(int64_t downloaded) {
  DCHECK(on_download_progress_callback_);

  // Make sure we're still in the right state since this is posted
  // asynchronously. In particular checking ->finished ensures that a partial
  // progress event isn't dispatched after the everything-is-loaded event
  // sent from FinishWithResult().
  if (!request_state_->body_started || request_state_->request_completed ||
      request_state_->finished) {
    return;
  }

  on_download_progress_callback_.Run(downloaded);
}

void SimpleURLLoaderImpl::FinishWithResult(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!request_state_->finished);

  client_receiver_.reset();
  url_loader_.reset();
  timeout_timer_.AbandonAndStop();

  request_state_->finished = true;
  request_state_->net_error = net_error;

  // Synthesize a final progress callback.
  if (on_download_progress_callback_) {
    base::WeakPtr<SimpleURLLoaderImpl> weak_this =
        weak_ptr_factory_.GetWeakPtr();
    on_download_progress_callback_.Run(GetContentSize());
    // If deleted by the callback, bail now.
    if (!weak_this)
      return;
  }

  // If it's a partial download or an error was received, erase the body.
  bool destroy_results =
      request_state_->net_error != net::OK && !allow_partial_results_;
  body_handler_->NotifyConsumerOfCompletion(destroy_results);
}

void SimpleURLLoaderImpl::Start(mojom::URLLoaderFactory* url_loader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(resource_request_);
  // It's illegal to use a single SimpleURLLoaderImpl to make multiple requests.
  DCHECK(!request_state_->finished);
  DCHECK(!url_loader_);
  DCHECK(!request_state_->body_started);

  // Stash the information if retries are enabled.
  if (remaining_retries_ > 0) {
    // Clone the URLLoaderFactory, to avoid any dependencies on its lifetime.
    // Results in an easier to use API, with no shutdown ordering requirements,
    // at the cost of some resources.
    url_loader_factory->Clone(
        url_loader_factory_remote_.BindNewPipeAndPassReceiver());
  }

  StartRequest(url_loader_factory);
}

void SimpleURLLoaderImpl::OnReadyToStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_loader_factory_remote_);
  StartRequest(url_loader_factory_remote_.get());
}

void SimpleURLLoaderImpl::StartRequest(
    mojom::URLLoaderFactory* url_loader_factory) {
  DCHECK(resource_request_);
  DCHECK(url_loader_factory);

  final_url_ = resource_request_->url;

  if (on_upload_progress_callback_)
    resource_request_->enable_upload_progress = true;

  // Data elements that use pipes aren't reuseable, currently (Since the IPC
  // code doesn't call the Clone() method), so need to create another one, if
  // uploading a string via a data pipe.
  if (string_upload_data_pipe_getter_) {
    resource_request_->request_body = new ResourceRequestBody();
    resource_request_->request_body->AppendDataPipe(
        string_upload_data_pipe_getter_->GetRemoteForNewUpload());
  }
  url_loader_factory->CreateLoaderAndStart(
      url_loader_.BindNewPipeAndPassReceiver(), request_id_,
      url_loader_factory_options_, *resource_request_,
      client_receiver_.BindNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(annotation_tag_));
  client_receiver_.set_disconnect_handler(base::BindOnce(
      &SimpleURLLoaderImpl::OnMojoDisconnect, base::Unretained(this)));

  // Note that this ends up restarting the timer on each retry.
  if (!timeout_duration_.is_zero()) {
    timeout_timer_.Start(
        FROM_HERE, timeout_duration_,
        base::BindOnce(&SimpleURLLoaderImpl::FinishWithResult,
                       weak_ptr_factory_.GetWeakPtr(), net::ERR_TIMED_OUT));
  }

  // If no more retries left, can clean up a little.
  if (remaining_retries_ == 0) {
    resource_request_.reset();
    url_loader_factory_remote_.reset();
  }
}

void SimpleURLLoaderImpl::Retry() {
  DCHECK(resource_request_);
  DCHECK(url_loader_factory_remote_);
  DCHECK_GT(remaining_retries_, 0);
  --remaining_retries_;
  ++num_retries_;

  client_receiver_.reset();
  url_loader_.reset();

  request_state_ = std::make_unique<RequestState>();
  body_handler_->PrepareToRetry(base::BindOnce(
      &SimpleURLLoaderImpl::StartRequest, weak_ptr_factory_.GetWeakPtr(),
      url_loader_factory_remote_.get()));
}

void SimpleURLLoaderImpl::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void SimpleURLLoaderImpl::OnReceiveResponse(
    mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request_state_->response_info) {
    // The final headers have already been received, so the URLLoader is
    // violating the API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }

  // Assume a 200 response unless headers were received indicating otherwise.
  // No headers indicates this was not a real HTTP response (Could be a file
  // URL, chrome URL, response could have been provided by something else, etc).
  int response_code = 200;
  if (response_head->headers)
    response_code = response_head->headers->response_code();

  // If a 5xx response was received, and |this| should retry on 5xx errors,
  // retry the request.
  if (response_code / 100 == 5 && remaining_retries_ > 0 &&
      (retry_mode_ & RETRY_ON_5XX)) {
    Retry();
    return;
  }

  base::WeakPtr<SimpleURLLoaderImpl> weak_this = weak_ptr_factory_.GetWeakPtr();
  if (on_response_started_callback_) {
    // Copy |final_url_| to a stack allocated GURL so it remains valid even if
    // the callback deletes |this|.
    GURL final_url = final_url_;
    std::move(on_response_started_callback_).Run(final_url, *response_head);
    // If deleted by the callback, bail now.
    if (!weak_this)
      return;
  }

  request_state_->response_info = std::move(response_head);
  if (!allow_http_error_results_ && response_code / 100 != 2) {
    FinishWithResult(net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    return;
  }

  if (!weak_this) {
    return;
  }

  if (!body) {
    if (!body_handler_->RequiresBodyDataPipe()) {
      // Fix up our state as if a body was read.
      request_state_->body_started = true;
      request_state_->body_completed = true;
      request_state_->received_body_size = kReceivedBodySizeUnknown;
    }
    return;
  }

  if (request_state_->body_started || !request_state_->response_info) {
    // If this was already called, or the headers have not been received,
    // the URLLoader is violating the API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }
  request_state_->body_started = true;
  body_handler_->OnStartLoadingResponseBody(std::move(body));
}

void SimpleURLLoaderImpl::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    mojom::URLResponseHeadPtr response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (request_state_->response_info) {
    // If the headers have already been received, the URLLoader is violating the
    // API contract.
    FinishWithResult(net::ERR_UNEXPECTED);
    return;
  }

  std::vector<std::string> removed_headers;
  if (on_redirect_callback_) {
    base::WeakPtr<SimpleURLLoaderImpl> weak_this =
        weak_ptr_factory_.GetWeakPtr();
    GURL url_before_redirect = final_url_;
    on_redirect_callback_.Run(url_before_redirect, redirect_info,
                              *response_head, &removed_headers);
    // If deleted by the callback, bail now.
    if (!weak_this)
      return;
  }

  final_url_ = redirect_info.new_url;
  url_loader_->FollowRedirect(removed_headers, {} /* modified_headers */,
                              {} /* modified_cors_exempt_headers */,
                              {} /* new_url */);
}

void SimpleURLLoaderImpl::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kSimpleURLLoaderImpl);
}

void SimpleURLLoaderImpl::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  if (on_upload_progress_callback_)
    on_upload_progress_callback_.Run(current_position, total_size);
  std::move(ack_callback).Run();
}

void SimpleURLLoaderImpl::OnComplete(const URLLoaderCompletionStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Request should not have been completed yet.
  DCHECK(!request_state_->finished);
  DCHECK(!request_state_->request_completed);

  // Reset pipes to ignore any subsequent close notification.
  client_receiver_.reset();
  url_loader_.reset();

  request_state_->completion_status = status;
  request_state_->request_completed = true;
  request_state_->net_error = status.error_code;
  request_state_->loaded_from_cache = status.exists_in_cache;
  // If |status| indicates success, but the body pipe was never received, the
  // URLLoader is violating the API contract.
  if (request_state_->net_error == net::OK && !request_state_->body_started) {
    request_state_->net_error = net::ERR_UNEXPECTED;
    request_state_->completion_status = std::nullopt;
  }

  MaybeComplete();
}

void SimpleURLLoaderImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |this| closes the pipe to the URLLoader in OnComplete(), so this method
  // being called indicates the pipe was closed before completion, most likely
  // due to peer death, or peer not calling OnComplete() on cancellation.

  // Request should not have been completed yet.
  DCHECK(!request_state_->finished);
  DCHECK(!request_state_->request_completed);
  DCHECK_EQ(net::ERR_IO_PENDING, request_state_->net_error);

  request_state_->request_completed = true;
  request_state_->net_error = net::ERR_FAILED;
  request_state_->completion_status = std::nullopt;

  // Wait to receive any pending data on the data pipe before reporting the
  // failure.
  MaybeComplete();
}

void SimpleURLLoaderImpl::MaybeComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Request should not have completed yet.
  DCHECK(!request_state_->finished);

  // Make sure the URLLoader's pipe has been closed.
  if (!request_state_->request_completed)
    return;

  // Make sure the body pipe either was never opened or has been closed. Even if
  // the request failed, if allow_partial_results_ is true, may still be able to
  // read more data.
  if (request_state_->body_started && !request_state_->body_completed)
    return;

  // DNS errors can be transient, and due to other issues, especially with
  // DoH. If required, retry.
  if (request_state_->net_error == net::ERR_NAME_NOT_RESOLVED &&
      remaining_retries_ > 0 && (retry_mode_ & RETRY_ON_NAME_NOT_RESOLVED)) {
    Retry();
    return;
  }

  // Retry on network change errors. Waiting for body complete isn't strictly
  // necessary, but it guarantees a consistent situation, with no reads pending
  // on the body pipe.
  if (request_state_->net_error == net::ERR_NETWORK_CHANGED &&
      remaining_retries_ > 0 && (retry_mode_ & RETRY_ON_NETWORK_CHANGE)) {
    Retry();
    return;
  }

  // If the URLLoader didn't supply a data pipe because we set the
  // ReadAndDiscardBody option, then we don't yet have a value for
  // `received_body_size`, so just set it to the size reported by URLLoader.
  if (request_state_->received_body_size == kReceivedBodySizeUnknown) {
    request_state_->received_body_size =
        request_state_->completion_status
            ? request_state_->completion_status->decoded_body_length
            : 0;
  }

  // When OnCompleted sees a success result, still need to report an error if
  // the size isn't what was expected.
  if (request_state_->net_error == net::OK &&
      request_state_->completion_status &&
      request_state_->completion_status->decoded_body_length !=
          request_state_->received_body_size) {
    if (request_state_->completion_status->decoded_body_length >
        request_state_->received_body_size) {
      // The body pipe was closed before it received the entire body.
      request_state_->net_error = net::ERR_FAILED;
      request_state_->completion_status = std::nullopt;
    } else {
      // The caller provided more data through the pipe than it reported in
      // URLLoaderCompletionStatus, so the URLLoader is violating the
      // API contract. Just fail the request and delete the retained completion
      // status.
      request_state_->net_error = net::ERR_UNEXPECTED;
      request_state_->completion_status = std::nullopt;
    }
  }

  FinishWithResult(request_state_->net_error);
}

}  // namespace

std::unique_ptr<SimpleURLLoader> SimpleURLLoader::Create(
    std::unique_ptr<ResourceRequest> resource_request,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    base::Location created_from) {
  DCHECK(resource_request);
  return std::make_unique<SimpleURLLoaderImpl>(std::move(resource_request),
                                               annotation_tag, created_from);
}

void SimpleURLLoader::SetTimeoutTickClockForTest(
    const base::TickClock* timeout_tick_clock) {
  timeout_tick_clock_ = timeout_tick_clock;
}

SimpleURLLoader::~SimpleURLLoader() {}

SimpleURLLoader::SimpleURLLoader() {}

}  // namespace network
