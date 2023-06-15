// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/form_data_bytes_consumer.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/blob_bytes_consumer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

enum class FormDataType {
  // Has only data elements.
  kSimple,
  // Can have data, file, and blob elements (no data pipes).
  kComplex,
  // Can have data and data pipe elements (no files and blobs).
  kDataPipeAndDataOnly
};

FormDataType GetType(const EncodedFormData* form_data) {
  FormDataType type = FormDataType::kSimple;
  for (const auto& element : form_data->Elements()) {
    switch (element.type_) {
      case FormDataElement::kData:
        break;
      case FormDataElement::kEncodedFile:
      case FormDataElement::kEncodedBlob:
        DCHECK_NE(type, FormDataType::kDataPipeAndDataOnly);
        type = FormDataType::kComplex;
        break;
      case FormDataElement::kDataPipe:
        DCHECK_NE(type, FormDataType::kComplex);
        type = FormDataType::kDataPipeAndDataOnly;
        break;
    }
  }
  return type;
}

class SimpleFormDataBytesConsumer : public BytesConsumer {
 public:
  explicit SimpleFormDataBytesConsumer(scoped_refptr<EncodedFormData> form_data)
      : form_data_(std::move(form_data)) {}

  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    if (form_data_) {
      form_data_->Flatten(flatten_form_data_);
      form_data_ = nullptr;
      DCHECK_EQ(flatten_form_data_offset_, 0u);
    }
    if (flatten_form_data_offset_ == flatten_form_data_.size())
      return Result::kDone;
    *buffer = flatten_form_data_.data() + flatten_form_data_offset_;
    *available = flatten_form_data_.size() - flatten_form_data_offset_;
    return Result::kOk;
  }
  Result EndRead(size_t read_size) override {
    DCHECK(!form_data_);
    DCHECK_LE(flatten_form_data_offset_ + read_size, flatten_form_data_.size());
    flatten_form_data_offset_ += read_size;
    if (flatten_form_data_offset_ == flatten_form_data_.size()) {
      state_ = PublicState::kClosed;
      return Result::kDone;
    }
    return Result::kOk;
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    if (!form_data_)
      return nullptr;

    Vector<char> data;
    form_data_->Flatten(data);
    form_data_ = nullptr;
    auto blob_data = std::make_unique<BlobData>();
    blob_data->AppendBytes(data.data(), data.size());
    auto length = blob_data->length();
    state_ = PublicState::kClosed;
    return BlobDataHandle::Create(std::move(blob_data), length);
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (!form_data_)
      return nullptr;

    state_ = PublicState::kClosed;
    return std::move(form_data_);
  }
  void SetClient(BytesConsumer::Client* client) override { DCHECK(client); }
  void ClearClient() override {}
  void Cancel() override {
    state_ = PublicState::kClosed;
    form_data_ = nullptr;
    flatten_form_data_.clear();
    flatten_form_data_offset_ = 0;
  }
  PublicState GetPublicState() const override { return state_; }
  Error GetError() const override {
    NOTREACHED();
    return Error();
  }
  String DebugName() const override { return "SimpleFormDataBytesConsumer"; }

 private:
  // Either one of |form_data_| and |flatten_form_data_| is usable at a time.
  scoped_refptr<EncodedFormData> form_data_;
  Vector<char> flatten_form_data_;
  size_t flatten_form_data_offset_ = 0;
  PublicState state_ = PublicState::kReadableOrWaiting;
};

class DataPipeAndDataBytesConsumer final : public BytesConsumer {
 public:
  DataPipeAndDataBytesConsumer(ExecutionContext* execution_context,
                               EncodedFormData* form_data)
      : execution_context_(execution_context) {
    // Make a copy in case |form_data| will mutate while we read it. Copy()
    // works fine; we don't need to DeepCopy() the data and data pipe getter:
    // data is just a Vector<char> and data pipe getter can be shared.
    form_data_ = form_data->Copy();
    form_data_->SetBoundary(FormDataEncoder::GenerateUniqueBoundaryString());
    iter_ = form_data_->MutableElements().begin();
  }

  Result BeginRead(const char** buffer, size_t* available) override {
    *buffer = nullptr;
    *available = 0;
    if (state_ == PublicState::kClosed)
      return Result::kDone;
    if (state_ == PublicState::kErrored)
      return Result::kError;

    if (iter_ == form_data_->MutableElements().end()) {
      Close();
      return Result::kDone;
    }

    // Currently reading bytes.
    if (iter_->type_ == FormDataElement::kData) {
      // Create the bytes consumer if there isn't one yet.
      if (!simple_consumer_) {
        scoped_refptr<EncodedFormData> simple_data =
            EncodedFormData::Create(iter_->data_);
        simple_consumer_ = MakeGarbageCollected<SimpleFormDataBytesConsumer>(
            std::move(simple_data));
        if (client_)
          simple_consumer_->SetClient(client_);
      }
      // Read from the bytes consumer.
      Result result = simple_consumer_->BeginRead(buffer, available);
      if (result == Result::kError) {
        SetError();
        return Result::kError;
      }
      // If done, continue to the next element.
      if (result == Result::kDone) {
        simple_consumer_ = nullptr;
        ++iter_;
        return BeginRead(buffer, available);
      }
      return result;
    }

    // Currently reading a data pipe.
    if (iter_->type_ == FormDataElement::kDataPipe) {
      // Create the data pipe consumer if there isn't one yet.
      if (!data_pipe_consumer_) {
        network::mojom::blink::DataPipeGetter* data_pipe_getter =
            iter_->data_pipe_getter_->GetDataPipeGetter();

        mojo::ScopedDataPipeProducerHandle pipe_producer_handle;
        mojo::ScopedDataPipeConsumerHandle pipe_consumer_handle;
        MojoResult rv = mojo::CreateDataPipe(nullptr, pipe_producer_handle,
                                             pipe_consumer_handle);
        if (rv != MOJO_RESULT_OK) {
          return Result::kError;
        }

        data_pipe_getter->Read(
            std::move(pipe_producer_handle),
            WTF::BindOnce(&DataPipeAndDataBytesConsumer::DataPipeGetterCallback,
                          WrapWeakPersistent(this)));
        DataPipeBytesConsumer::CompletionNotifier* completion_notifier =
            nullptr;
        data_pipe_consumer_ = MakeGarbageCollected<DataPipeBytesConsumer>(
            execution_context_->GetTaskRunner(TaskType::kNetworking),
            std::move(pipe_consumer_handle), &completion_notifier);
        completion_notifier_ = completion_notifier;
        if (client_)
          data_pipe_consumer_->SetClient(client_);
      }

      // Read from the data pipe consumer.
      Result result = data_pipe_consumer_->BeginRead(buffer, available);
      if (result == Result::kError) {
        SetError();
        return Result::kError;
      }

      if (result == Result::kDone) {
        // We're done. Move on to the next element.
        data_pipe_consumer_ = nullptr;
        completion_notifier_ = nullptr;
        ++iter_;
        return BeginRead(buffer, available);
      }
      return result;
    }

    NOTREACHED() << "Invalid type: " << iter_->type_;
    return Result::kError;
  }

  Result EndRead(size_t read_size) override {
    if (state_ == PublicState::kClosed)
      return Result::kDone;
    if (state_ == PublicState::kErrored)
      return Result::kError;

    if (simple_consumer_) {
      Result result = simple_consumer_->EndRead(read_size);
      if (result == Result::kError) {
        SetError();
        return Result::kError;
      }
      // Even if this consumer is done, there may still be more elements, so
      // return Ok.
      DCHECK(result == Result::kOk || result == Result::kDone);
      return Result::kOk;
    }
    if (data_pipe_consumer_) {
      Result result = data_pipe_consumer_->EndRead(read_size);
      if (result == Result::kError) {
        SetError();
        return Result::kError;
      }
      // Even if this consumer is done, there may still be more elements, so
      // return Ok.
      DCHECK(result == Result::kOk || result == Result::kDone);
      return Result::kOk;
    }

    NOTREACHED() << "No consumer. BeginRead() was not called?";
    return Result::kError;
  }

  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (state_ == PublicState::kClosed || state_ == PublicState::kErrored)
      return nullptr;
    // According to the DrainAsFormData() contract, we can only return bytes
    // that haven't already been read. So if reading has already started,
    // give up and return null.
    if (simple_consumer_ || data_pipe_consumer_)
      return nullptr;
    Close();
    return std::move(form_data_);
  }

  void SetClient(Client* client) override {
    DCHECK(!client_);
    DCHECK(client);
    client_ = client;
    if (simple_consumer_)
      simple_consumer_->SetClient(client_);
    else if (data_pipe_consumer_)
      data_pipe_consumer_->SetClient(client_);
  }

  void ClearClient() override {
    client_ = nullptr;
    if (simple_consumer_)
      simple_consumer_->ClearClient();
    else if (data_pipe_consumer_)
      data_pipe_consumer_->ClearClient();
  }

  void Cancel() override {
    if (state_ == PublicState::kClosed || state_ == PublicState::kErrored)
      return;
    if (simple_consumer_)
      simple_consumer_->Cancel();
    else if (data_pipe_consumer_)
      data_pipe_consumer_->Cancel();
    Close();
  }

  PublicState GetPublicState() const override { return state_; }

  Error GetError() const override {
    DCHECK_EQ(state_, PublicState::kErrored);
    return error_;
  }

  String DebugName() const override { return "DataPipeAndDataBytesConsumer"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(execution_context_);
    visitor->Trace(client_);
    visitor->Trace(simple_consumer_);
    visitor->Trace(data_pipe_consumer_);
    visitor->Trace(completion_notifier_);
    BytesConsumer::Trace(visitor);
  }

 private:
  void DataPipeGetterCallback(int32_t status, uint64_t size) {
    switch (state_) {
      case PublicState::kErrored:
        // The error should have already been propagated to the notifier.
        DCHECK(!completion_notifier_);
        DCHECK(!data_pipe_consumer_);
        return;
      case PublicState::kClosed:
        // The data_pipe_consumer_ should already be cleaned up.
        DCHECK(!completion_notifier_);
        DCHECK(!data_pipe_consumer_);
        return;
      case PublicState::kReadableOrWaiting:
        break;
    }

    DCHECK(completion_notifier_);
    if (status != 0) {
      // 0 is net::OK.
      completion_notifier_->SignalError(Error("error"));
    } else {
      completion_notifier_->SignalComplete();
    }
  }

  void Close() {
    if (state_ == PublicState::kClosed)
      return;
    DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
    state_ = PublicState::kClosed;
    ClearClient();
    simple_consumer_ = nullptr;
    if (data_pipe_consumer_) {
      data_pipe_consumer_->Cancel();
      data_pipe_consumer_ = nullptr;
      completion_notifier_ = nullptr;
    }
  }

  void SetError() {
    if (state_ == PublicState::kErrored)
      return;
    DCHECK_EQ(state_, PublicState::kReadableOrWaiting);
    state_ = PublicState::kErrored;
    error_ = Error("error");
    ClearClient();
    simple_consumer_ = nullptr;
    if (completion_notifier_) {
      completion_notifier_->SignalError(error_);
      completion_notifier_ = nullptr;
      data_pipe_consumer_ = nullptr;
    }
  }

  Member<ExecutionContext> execution_context_;
  PublicState state_ = PublicState::kReadableOrWaiting;
  scoped_refptr<EncodedFormData> form_data_;
  Vector<FormDataElement>::iterator iter_;
  Error error_;
  Member<BytesConsumer::Client> client_;
  Member<SimpleFormDataBytesConsumer> simple_consumer_;
  Member<DataPipeBytesConsumer> data_pipe_consumer_;
  Member<DataPipeBytesConsumer::CompletionNotifier> completion_notifier_;
};

class ComplexFormDataBytesConsumer final : public BytesConsumer {
 public:
  ComplexFormDataBytesConsumer(ExecutionContext* execution_context,
                               scoped_refptr<EncodedFormData> form_data,
                               BytesConsumer* consumer_for_testing)
      : form_data_(std::move(form_data)) {
    if (consumer_for_testing) {
      blob_bytes_consumer_ = consumer_for_testing;
      return;
    }

    auto blob_data = std::make_unique<BlobData>();
    for (const auto& element : form_data_->Elements()) {
      switch (element.type_) {
        case FormDataElement::kData:
          blob_data->AppendBytes(element.data_.data(), element.data_.size());
          break;
        case FormDataElement::kEncodedFile: {
          auto file_length = element.file_length_;
          if (file_length < 0) {
            if (!GetFileSize(element.filename_, *execution_context,
                             file_length)) {
              form_data_ = nullptr;
              blob_bytes_consumer_ = BytesConsumer::CreateErrored(
                  Error("Cannot determine a file size"));
              return;
            }
          }
          blob_data->AppendBlob(
              BlobDataHandle::CreateForFile(
                  element.filename_, element.file_start_, file_length,
                  element.expected_file_modification_time_,
                  /*content_type=*/""),
              0, file_length);
          break;
        }
        case FormDataElement::kEncodedBlob:
          if (element.optional_blob_data_handle_) {
            blob_data->AppendBlob(element.optional_blob_data_handle_, 0,
                                  element.optional_blob_data_handle_->size());
          }
          break;
        case FormDataElement::kDataPipe:
          NOTREACHED() << "This consumer can't handle data pipes.";
          break;
      }
    }
    // Here we handle m_formData->boundary() as a C-style string. See
    // FormDataEncoder::generateUniqueBoundaryString.
    blob_data->SetContentType(AtomicString("multipart/form-data; boundary=") +
                              form_data_->Boundary().data());
    auto size = blob_data->length();
    blob_bytes_consumer_ = MakeGarbageCollected<BlobBytesConsumer>(
        execution_context, BlobDataHandle::Create(std::move(blob_data), size));
  }

  // BytesConsumer implementation
  Result BeginRead(const char** buffer, size_t* available) override {
    form_data_ = nullptr;
    // Delegate the operation to the underlying consumer. This relies on
    // the fact that we appropriately notify the draining information to
    // the underlying consumer.
    return blob_bytes_consumer_->BeginRead(buffer, available);
  }
  Result EndRead(size_t read_size) override {
    return blob_bytes_consumer_->EndRead(read_size);
  }
  scoped_refptr<BlobDataHandle> DrainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    scoped_refptr<BlobDataHandle> handle =
        blob_bytes_consumer_->DrainAsBlobDataHandle(policy);
    if (handle)
      form_data_ = nullptr;
    return handle;
  }
  scoped_refptr<EncodedFormData> DrainAsFormData() override {
    if (!form_data_)
      return nullptr;
    blob_bytes_consumer_->Cancel();
    return std::move(form_data_);
  }
  void SetClient(BytesConsumer::Client* client) override {
    blob_bytes_consumer_->SetClient(client);
  }
  void ClearClient() override { blob_bytes_consumer_->ClearClient(); }
  void Cancel() override {
    form_data_ = nullptr;
    blob_bytes_consumer_->Cancel();
  }
  PublicState GetPublicState() const override {
    return blob_bytes_consumer_->GetPublicState();
  }
  Error GetError() const override { return blob_bytes_consumer_->GetError(); }
  String DebugName() const override { return "ComplexFormDataBytesConsumer"; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(blob_bytes_consumer_);
    BytesConsumer::Trace(visitor);
  }

 private:
  scoped_refptr<EncodedFormData> form_data_;
  Member<BytesConsumer> blob_bytes_consumer_;
};

}  // namespace

FormDataBytesConsumer::FormDataBytesConsumer(const String& string)
    : impl_(MakeGarbageCollected<SimpleFormDataBytesConsumer>(
          EncodedFormData::Create(
              UTF8Encoding().Encode(string, WTF::kNoUnencodables)))) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBuffer* buffer)
    : FormDataBytesConsumer(
          buffer->Data(),
          base::checked_cast<wtf_size_t>(buffer->ByteLength())) {}

FormDataBytesConsumer::FormDataBytesConsumer(DOMArrayBufferView* view)
    : FormDataBytesConsumer(
          view->BaseAddress(),
          base::checked_cast<wtf_size_t>(view->byteLength())) {}

FormDataBytesConsumer::FormDataBytesConsumer(const void* data, wtf_size_t size)
    : impl_(MakeGarbageCollected<SimpleFormDataBytesConsumer>(
          EncodedFormData::Create(data, size))) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* execution_context,
    scoped_refptr<EncodedFormData> form_data)
    : FormDataBytesConsumer(execution_context, std::move(form_data), nullptr) {}

FormDataBytesConsumer::FormDataBytesConsumer(
    ExecutionContext* execution_context,
    scoped_refptr<EncodedFormData> form_data,
    BytesConsumer* consumer_for_testing)
    : impl_(GetImpl(execution_context,
                    std::move(form_data),
                    consumer_for_testing)) {}

// static
BytesConsumer* FormDataBytesConsumer::GetImpl(
    ExecutionContext* execution_context,
    scoped_refptr<EncodedFormData> form_data,
    BytesConsumer* consumer_for_testing) {
  DCHECK(form_data);
  switch (GetType(form_data.get())) {
    case FormDataType::kSimple:
      return MakeGarbageCollected<SimpleFormDataBytesConsumer>(
          std::move(form_data));
    case FormDataType::kComplex:
      return MakeGarbageCollected<ComplexFormDataBytesConsumer>(
          execution_context, std::move(form_data), consumer_for_testing);
    case FormDataType::kDataPipeAndDataOnly:
      return MakeGarbageCollected<DataPipeAndDataBytesConsumer>(
          execution_context, form_data.get());
  }
  return nullptr;
}

}  // namespace blink
