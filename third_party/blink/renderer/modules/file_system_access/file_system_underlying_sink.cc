// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_underlying_sink.h"

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_blob_usvstring.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_arraybuffer_arraybufferview_blob_usvstring_writeparams.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_write_command_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_write_params.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

FileSystemUnderlyingSink::FileSystemUnderlyingSink(
    ExecutionContext* context,
    mojo::PendingRemote<mojom::blink::FileSystemAccessFileWriter> writer_remote)
    : writer_remote_(context) {
  writer_remote_.Bind(std::move(writer_remote),
                      context->GetTaskRunner(TaskType::kStorage));
  DCHECK(writer_remote_.is_bound());
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  auto* input = NativeValueTraits<
      V8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVStringOrWriteParams>::
      NativeValue(script_state->GetIsolate(), chunk.V8Value(), exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  if (input->IsWriteParams()) {
    return HandleParams(script_state, *input->GetAsWriteParams(),
                        exception_state);
  }

  auto* write_data =
      input->GetAsV8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVString();
  return WriteData(script_state, offset_, write_data, exception_state);
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    ThrowDOMExceptionAndInvalidateSink(exception_state,
                                       DOMExceptionCode::kInvalidStateError,
                                       "Object reached an invalid state");
    return EmptyPromise();
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  auto result = pending_operation_->Promise();
  writer_remote_->Close(WTF::BindOnce(&FileSystemUnderlyingSink::CloseComplete,
                                      WrapPersistent(this)));

  return result;
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been aborted. Terminating the remote connection
  // will ensure that the writes are not closed successfully.
  if (writer_remote_.is_bound())
    writer_remote_.reset();
  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::HandleParams(
    ScriptState* script_state,
    const WriteParams& params,
    ExceptionState& exception_state) {
  if (params.type() == V8WriteCommandType::Enum::kTruncate) {
    if (!params.hasSizeNonNull()) {
      ThrowDOMExceptionAndInvalidateSink(
          exception_state, DOMExceptionCode::kSyntaxError,
          "Invalid params passed. truncate requires a size argument");
      return EmptyPromise();
    }
    return Truncate(script_state, params.sizeNonNull(), exception_state);
  }

  if (params.type() == V8WriteCommandType::Enum::kSeek) {
    if (!params.hasPositionNonNull()) {
      ThrowDOMExceptionAndInvalidateSink(
          exception_state, DOMExceptionCode::kSyntaxError,
          "Invalid params passed. seek requires a position argument");
      return EmptyPromise();
    }
    return Seek(script_state, params.positionNonNull(), exception_state);
  }

  if (params.type() == V8WriteCommandType::Enum::kWrite) {
    uint64_t position =
        params.hasPositionNonNull() ? params.positionNonNull() : offset_;
    if (!params.hasData()) {
      ThrowDOMExceptionAndInvalidateSink(
          exception_state, DOMExceptionCode::kSyntaxError,
          "Invalid params passed. write requires a data argument");
      return EmptyPromise();
    }
    if (!params.data()) {
      ThrowTypeErrorAndInvalidateSink(
          exception_state,
          "Invalid params passed. write requires a non-null data");
      return EmptyPromise();
    }
    return WriteData(script_state, position, params.data(), exception_state);
  }

  ThrowDOMExceptionAndInvalidateSink(exception_state,
                                     DOMExceptionCode::kInvalidStateError,
                                     "Object reached an invalid state");
  return EmptyPromise();
}

namespace {
// Write operations generally consist of two separate operations, both of which
// can result in an error:
// 1) The data producer (be it a Blob or mojo::DataPipeProducer) writes data to
//    a data pipe.
// 2) The browser side file writer implementation reads data from the data pipe,
//    and writes this to the file.
//
// Both operations can report errors in either order, and we have to wait for
// both to report success before we can consider the combined write call to have
// succeeded. This helper class listens for both complete events and signals
// success when both succeeded, or an error when either operation failed.
//
// This class deletes itself after calling its callback.
class WriterHelper {
 public:
  explicit WriterHelper(
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr result,
                              uint64_t bytes_written)> callback)
      : callback_(std::move(callback)) {}
  virtual ~WriterHelper() = default;

  // This method is called in response to the mojom Write call. It reports the
  // result of the write operation from the point of view of the file system API
  // implementation.
  void WriteComplete(mojom::blink::FileSystemAccessErrorPtr result,
                     uint64_t bytes_written) {
    DCHECK(!write_result_);
    write_result_ = std::move(result);
    bytes_written_ = bytes_written;
    MaybeCallCallbackAndDeleteThis();
  }

  // This method is called by renderer side code (in subclasses of this class)
  // when we've finished producing data to be written.
  void ProducerComplete(mojom::blink::FileSystemAccessErrorPtr result) {
    DCHECK(!producer_result_);
    producer_result_ = std::move(result);
    MaybeCallCallbackAndDeleteThis();
  }

  virtual base::WeakPtr<WriterHelper> AsWeakPtr() = 0;

 private:
  void MaybeCallCallbackAndDeleteThis() {
    DCHECK(callback_);

    if (!producer_result_.is_null() &&
        producer_result_->status != mojom::blink::FileSystemAccessStatus::kOk) {
      // Producing data failed, report that error.
      std::move(callback_).Run(std::move(producer_result_), bytes_written_);
      delete this;
      return;
    }

    if (!write_result_.is_null() &&
        write_result_->status != mojom::blink::FileSystemAccessStatus::kOk) {
      // Writing failed, report that error.
      std::move(callback_).Run(std::move(write_result_), bytes_written_);
      delete this;
      return;
    }

    if (!producer_result_.is_null() && !write_result_.is_null()) {
      // Both operations succeeded, report success.
      std::move(callback_).Run(std::move(write_result_), bytes_written_);
      delete this;
      return;
    }

    // Still waiting for the other operation to complete, so don't call the
    // callback yet.
  }

  base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr result,
                          uint64_t bytes_written)>
      callback_;

  mojom::blink::FileSystemAccessErrorPtr producer_result_;
  mojom::blink::FileSystemAccessErrorPtr write_result_;
  uint64_t bytes_written_ = 0;
};

// WriterHelper implementation that is used when data is being produced by a
// mojo::DataPipeProducer, generally because the data was passed in as an
// ArrayBuffer or String.
class StreamWriterHelper final : public WriterHelper {
 public:
  StreamWriterHelper(
      std::unique_ptr<mojo::DataPipeProducer> producer,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr result,
                              uint64_t bytes_written)> callback)
      : WriterHelper(std::move(callback)), producer_(std::move(producer)) {}

  void DataProducerComplete(MojoResult result) {
    // Reset `producer_` to close the DataPipe. Without this the Write operation
    // will never complete as it will keep waiting for more data.
    producer_ = nullptr;

    if (result == MOJO_RESULT_OK) {
      ProducerComplete(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kOk, base::File::FILE_OK, ""));
    } else {
      ProducerComplete(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kOperationAborted,
          base::File::FILE_OK, "Failed to write data to data pipe"));
    }
  }

  base::WeakPtr<WriterHelper> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<mojo::DataPipeProducer> producer_;
  base::WeakPtrFactory<WriterHelper> weak_ptr_factory_{this};
};

// WriterHelper implementation that is used when data is being produced by a
// Blob.
class BlobWriterHelper final : public mojom::blink::BlobReaderClient,
                               public WriterHelper {
 public:
  BlobWriterHelper(
      mojo::PendingReceiver<mojom::blink::BlobReaderClient> receiver,
      base::OnceCallback<void(mojom::blink::FileSystemAccessErrorPtr result,
                              uint64_t bytes_written)> callback)
      : WriterHelper(std::move(callback)),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        WTF::BindOnce(&BlobWriterHelper::OnDisconnect, WTF::Unretained(this)));
  }

  // BlobReaderClient:
  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override {
    complete_called_ = true;
    // This error conversion matches what FileReaderLoader does. Failing to read
    // a blob using FileReader should result in the same exception type as
    // failing to read a blob here.
    if (status == net::OK) {
      ProducerComplete(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kOk, base::File::FILE_OK, ""));
    } else if (status == net::ERR_FILE_NOT_FOUND) {
      ProducerComplete(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kFileError,
          base::File::FILE_ERROR_NOT_FOUND, ""));
    } else {
      ProducerComplete(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kFileError,
          base::File::FILE_ERROR_IO, ""));
    }
  }

  base::WeakPtr<WriterHelper> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnDisconnect() {
    if (!complete_called_) {
      // Disconnected without getting a read result, treat this as read failure.
      ProducerComplete(mojom::blink::FileSystemAccessError::New(
          mojom::blink::FileSystemAccessStatus::kOperationAborted,
          base::File::FILE_OK, "Blob disconnected while reading"));
    }
  }

  mojo::Receiver<mojom::blink::BlobReaderClient> receiver_;
  bool complete_called_ = false;
  base::WeakPtrFactory<WriterHelper> weak_ptr_factory_{this};
};

// Creates a mojo data pipe, where the capacity of the data pipe is derived from
// the provided `data_size`. Returns false and throws an exception if creating
// the data pipe failed.
bool CreateDataPipe(uint64_t data_size,
                    ExceptionState& exception_state,
                    mojo::ScopedDataPipeProducerHandle& producer,
                    mojo::ScopedDataPipeConsumerHandle& consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = BlobUtils::GetDataPipeCapacity(data_size);

  MojoResult rv = CreateDataPipe(&options, producer, consumer);
  return rv == MOJO_RESULT_OK;
}

}  // namespace

void FileSystemUnderlyingSink::ThrowDOMExceptionAndInvalidateSink(
    ExceptionState& exception_state,
    DOMExceptionCode error,
    const char* message) {
  exception_state.ThrowDOMException(error, message);
  writer_remote_.reset();
}

void FileSystemUnderlyingSink::ThrowTypeErrorAndInvalidateSink(
    ExceptionState& exception_state,
    const char* message) {
  exception_state.ThrowTypeError(message);
  writer_remote_.reset();
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::WriteData(
    ScriptState* script_state,
    uint64_t position,
    const V8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVString* data,
    ExceptionState& exception_state) {
  DCHECK(data);

  if (!writer_remote_.is_bound() || pending_operation_) {
    ThrowDOMExceptionAndInvalidateSink(exception_state,
                                       DOMExceptionCode::kInvalidStateError,
                                       "Object reached an invalid state");
    return EmptyPromise();
  }

  offset_ = position;
  std::unique_ptr<mojo::DataPipeProducer::DataSource> data_source;
  switch (data->GetContentType()) {
    case V8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVString::ContentType::
        kArrayBuffer: {
      DOMArrayBuffer* array_buffer = data->GetAsArrayBuffer();
      data_source = std::make_unique<mojo::StringDataSource>(
          base::as_chars(array_buffer->ByteSpan()),
          mojo::StringDataSource::AsyncWritingMode::
              STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
      break;
    }
    case V8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVString::ContentType::
        kArrayBufferView: {
      DOMArrayBufferView* array_buffer_view =
          data->GetAsArrayBufferView().Get();
      data_source = std::make_unique<mojo::StringDataSource>(
          base::as_chars(array_buffer_view->ByteSpan()),
          mojo::StringDataSource::AsyncWritingMode::
              STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
      break;
    }
    case V8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVString::ContentType::
        kBlob:
      break;
    case V8UnionArrayBufferOrArrayBufferViewOrBlobOrUSVString::ContentType::
        kUSVString:
      data_source = std::make_unique<mojo::StringDataSource>(
          StringUTF8Adaptor(data->GetAsUSVString()).AsStringView(),
          mojo::StringDataSource::AsyncWritingMode::
              STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION);
      break;
  }

  DCHECK(data_source || data->IsBlob());
  uint64_t data_size =
      data_source ? data_source->GetLength() : data->GetAsBlob()->size();

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (!CreateDataPipe(data_size, exception_state, producer_handle,
                      consumer_handle)) {
    ThrowDOMExceptionAndInvalidateSink(exception_state,
                                       DOMExceptionCode::kInvalidStateError,
                                       "Failed to create datapipe");
    return EmptyPromise();
  }

  WriterHelper* helper;
  if (data->IsBlob()) {
    mojo::PendingRemote<mojom::blink::BlobReaderClient> reader_client;
    helper = new BlobWriterHelper(
        reader_client.InitWithNewPipeAndPassReceiver(),
        WTF::BindOnce(&FileSystemUnderlyingSink::WriteComplete,
                      WrapPersistent(this)));
    data->GetAsBlob()->GetBlobDataHandle()->ReadAll(std::move(producer_handle),
                                                    std::move(reader_client));
  } else {
    auto producer =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    auto* producer_ptr = producer.get();
    helper = new StreamWriterHelper(
        std::move(producer),
        WTF::BindOnce(&FileSystemUnderlyingSink::WriteComplete,
                      WrapPersistent(this)));
    // Unretained is safe because the producer is owned by `helper`.
    producer_ptr->Write(
        std::move(data_source),
        WTF::BindOnce(
            &StreamWriterHelper::DataProducerComplete,
            WTF::Unretained(static_cast<StreamWriterHelper*>(helper))));
  }

  writer_remote_->Write(
      position, std::move(consumer_handle),
      WTF::BindOnce(&WriterHelper::WriteComplete, helper->AsWeakPtr()));

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  return pending_operation_->Promise();
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::Truncate(
    ScriptState* script_state,
    uint64_t size,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    ThrowDOMExceptionAndInvalidateSink(exception_state,
                                       DOMExceptionCode::kInvalidStateError,
                                       "Object reached an invalid state");
    return EmptyPromise();
  }
  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  auto result = pending_operation_->Promise();
  writer_remote_->Truncate(
      size, WTF::BindOnce(&FileSystemUnderlyingSink::TruncateComplete,
                          WrapPersistent(this), size));
  return result;
}

ScriptPromise<IDLUndefined> FileSystemUnderlyingSink::Seek(
    ScriptState* script_state,
    uint64_t offset,
    ExceptionState& exception_state) {
  if (!writer_remote_.is_bound() || pending_operation_) {
    ThrowDOMExceptionAndInvalidateSink(exception_state,
                                       DOMExceptionCode::kInvalidStateError,
                                       "Object reached an invalid state");
    return EmptyPromise();
  }
  offset_ = offset;
  return ToResolvedUndefinedPromise(script_state);
}

void FileSystemUnderlyingSink::WriteComplete(
    mojom::blink::FileSystemAccessErrorPtr result,
    uint64_t bytes_written) {
  DCHECK(pending_operation_);

  if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
    // Advance offset.
    offset_ += bytes_written;

    pending_operation_->Resolve();
  } else {
    // An error of any kind puts the underlying stream into an unrecoverable
    // error state. See https://crbug.com/1380650#c5. Close the mojo pipe to
    // clean up resources held by the browser process - including the file lock.
    writer_remote_.reset();
    file_system_access_error::Reject(pending_operation_, *result);
  }
  pending_operation_ = nullptr;
}

void FileSystemUnderlyingSink::TruncateComplete(
    uint64_t to_size,
    mojom::blink::FileSystemAccessErrorPtr result) {
  DCHECK(pending_operation_);

  if (result->status == mojom::blink::FileSystemAccessStatus::kOk) {
    // Set offset to smallest last set size so that a subsequent write is not
    // out of bounds.
    offset_ = to_size < offset_ ? to_size : offset_;

    pending_operation_->Resolve();
  } else {
    // An error of any kind puts the underlying stream into an unrecoverable
    // error state. See https://crbug.com/1380650#c5. Close the mojo pipe to
    // clean up resources held by the browser process - including the file lock.
    writer_remote_.reset();
    file_system_access_error::Reject(pending_operation_, *result);
  }
  pending_operation_ = nullptr;
}

void FileSystemUnderlyingSink::CloseComplete(
    mojom::blink::FileSystemAccessErrorPtr result) {
  DCHECK(pending_operation_);

  // We close the mojo pipe because we intend this writable file stream to be
  // discarded after close. Subsequent operations will fail.
  writer_remote_.reset();

  file_system_access_error::ResolveOrReject(pending_operation_, *result);
  pending_operation_ = nullptr;
}

void FileSystemUnderlyingSink::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  UnderlyingSinkBase::Trace(visitor);
  visitor->Trace(writer_remote_);
  visitor->Trace(pending_operation_);
}

}  // namespace blink
