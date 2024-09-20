/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2012 Digia Plc. and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/platform/network/encoded_form_data.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

FormDataElement::FormDataElement() : type_(kData) {}

FormDataElement::FormDataElement(const Vector<char>& array)
    : type_(kData), data_(array) {}

FormDataElement::FormDataElement(Vector<char>&& array)
    : type_(kData), data_(std::move(array)) {}

FormDataElement::FormDataElement(
    const String& filename,
    int64_t file_start,
    int64_t file_length,
    const std::optional<base::Time>& expected_file_modification_time)
    : type_(kEncodedFile),
      filename_(filename),
      file_start_(file_start),
      file_length_(file_length),
      expected_file_modification_time_(expected_file_modification_time) {}

FormDataElement::FormDataElement(const String& blob_uuid,
                                 scoped_refptr<BlobDataHandle> optional_handle)
    : type_(kEncodedBlob),
      blob_uuid_(blob_uuid),
      optional_blob_data_handle_(std::move(optional_handle)) {
  DCHECK(optional_blob_data_handle_);
}

FormDataElement::FormDataElement(
    scoped_refptr<WrappedDataPipeGetter> data_pipe_getter)
    : type_(kDataPipe), data_pipe_getter_(std::move(data_pipe_getter)) {}

FormDataElement::FormDataElement(const FormDataElement&) = default;
FormDataElement::FormDataElement(FormDataElement&&) = default;
FormDataElement::~FormDataElement() = default;
FormDataElement& FormDataElement::operator=(const FormDataElement&) = default;
FormDataElement& FormDataElement::operator=(FormDataElement&&) = default;

bool operator==(const FormDataElement& a, const FormDataElement& b) {
  if (&a == &b)
    return true;

  if (a.type_ != b.type_)
    return false;
  if (a.type_ == FormDataElement::kData)
    return a.data_ == b.data_;
  if (a.type_ == FormDataElement::kEncodedFile) {
    return a.filename_ == b.filename_ && a.file_start_ == b.file_start_ &&
           a.file_length_ == b.file_length_ &&
           a.expected_file_modification_time_ ==
               b.expected_file_modification_time_;
  }
  if (a.type_ == FormDataElement::kEncodedBlob)
    return a.blob_uuid_ == b.blob_uuid_;
  if (a.type_ == FormDataElement::kDataPipe)
    return a.data_pipe_getter_ == b.data_pipe_getter_;

  return true;
}

inline EncodedFormData::EncodedFormData()
    : identifier_(0), contains_password_data_(false) {}

inline EncodedFormData::EncodedFormData(const EncodedFormData& data)
    : RefCounted<EncodedFormData>(),
      elements_(data.elements_),
      identifier_(data.identifier_),
      contains_password_data_(data.contains_password_data_) {}

EncodedFormData::~EncodedFormData() = default;

scoped_refptr<EncodedFormData> EncodedFormData::Create() {
  return base::AdoptRef(new EncodedFormData);
}

scoped_refptr<EncodedFormData> EncodedFormData::Create(const void* data,
                                                       wtf_size_t size) {
  scoped_refptr<EncodedFormData> result = Create();
  result->AppendData(data, size);
  return result;
}

scoped_refptr<EncodedFormData> EncodedFormData::Create(
    base::span<const char> string) {
  scoped_refptr<EncodedFormData> result = Create();
  result->AppendData(string.data(),
                     base::checked_cast<wtf_size_t>(string.size()));
  return result;
}

scoped_refptr<EncodedFormData> EncodedFormData::Create(SegmentedBuffer&& data) {
  scoped_refptr<EncodedFormData> result = Create();
  result->AppendData(std::move(data));
  return result;
}

scoped_refptr<EncodedFormData> EncodedFormData::Copy() const {
  return base::AdoptRef(new EncodedFormData(*this));
}

scoped_refptr<EncodedFormData> EncodedFormData::DeepCopy() const {
  scoped_refptr<EncodedFormData> form_data(Create());

  form_data->identifier_ = identifier_;
  form_data->boundary_ = boundary_;
  form_data->contains_password_data_ = contains_password_data_;

  form_data->elements_.ReserveInitialCapacity(elements_.size());
  for (const FormDataElement& e : elements_) {
    switch (e.type_) {
      case FormDataElement::kData:
        form_data->elements_.UncheckedAppend(FormDataElement(e.data_));
        break;
      case FormDataElement::kEncodedFile:
        form_data->elements_.UncheckedAppend(
            FormDataElement(e.filename_, e.file_start_, e.file_length_,
                            e.expected_file_modification_time_));
        break;
      case FormDataElement::kEncodedBlob:
        form_data->elements_.UncheckedAppend(
            FormDataElement(e.blob_uuid_, e.optional_blob_data_handle_));
        break;
      case FormDataElement::kDataPipe:
        mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
            data_pipe_getter;
        e.data_pipe_getter_->GetDataPipeGetter()->Clone(
            data_pipe_getter.InitWithNewPipeAndPassReceiver());
        auto wrapped = base::MakeRefCounted<WrappedDataPipeGetter>(
            std::move(data_pipe_getter));
        form_data->elements_.UncheckedAppend(
            FormDataElement(std::move(wrapped)));
        break;
    }
  }
  return form_data;
}

EncodedFormData::FormDataType EncodedFormData::GetType() const {
  FormDataType type = FormDataType::kDataOnly;
  for (const auto& element : Elements()) {
    switch (element.type_) {
      case FormDataElement::kData:
        break;
      case FormDataElement::kEncodedFile:
      case FormDataElement::kEncodedBlob:
        if (type == FormDataType::kDataAndDataPipe) {
          return FormDataType::kInvalid;
        }
        type = FormDataType::kDataAndEncodedFileOrBlob;
        break;
      case FormDataElement::kDataPipe:
        if (type == FormDataType::kDataAndEncodedFileOrBlob) {
          return FormDataType::kInvalid;
        }
        type = FormDataType::kDataAndDataPipe;
        break;
    }
  }
  return type;
}

void EncodedFormData::AppendData(const void* data, wtf_size_t size) {
  if (elements_.empty() || elements_.back().type_ != FormDataElement::kData)
    elements_.push_back(FormDataElement());
  FormDataElement& e = elements_.back();
  e.data_.Append(static_cast<const char*>(data), size);
}

void EncodedFormData::AppendData(SegmentedBuffer&& buffer) {
  Vector<Vector<char>> data_list = std::move(buffer).TakeData();
  for (auto& data : data_list) {
    elements_.push_back(FormDataElement(std::move(data)));
  }
}

void EncodedFormData::AppendFile(
    const String& filename,
    const std::optional<base::Time>& expected_modification_time) {
  elements_.push_back(FormDataElement(filename, 0, BlobData::kToEndOfFile,
                                      expected_modification_time));
}

void EncodedFormData::AppendFileRange(
    const String& filename,
    int64_t start,
    int64_t length,
    const std::optional<base::Time>& expected_modification_time) {
  elements_.push_back(
      FormDataElement(filename, start, length, expected_modification_time));
}

void EncodedFormData::AppendBlob(
    const String& uuid,
    scoped_refptr<BlobDataHandle> optional_handle) {
  elements_.push_back(FormDataElement(uuid, std::move(optional_handle)));
}

void EncodedFormData::AppendDataPipe(
    scoped_refptr<WrappedDataPipeGetter> handle) {
  elements_.emplace_back(std::move(handle));
}

void EncodedFormData::Flatten(Vector<char>& data) const {
  // Concatenate all the byte arrays, but omit everything else.
  data.clear();
  for (const FormDataElement& e : elements_) {
    if (e.type_ == FormDataElement::kData)
      data.AppendVector(e.data_);
  }
}

String EncodedFormData::FlattenToString() const {
  Vector<char> bytes;
  Flatten(bytes);
  return Latin1Encoding().Decode(base::as_byte_span(bytes));
}

uint64_t EncodedFormData::SizeInBytes() const {
  unsigned size = 0;
  for (const FormDataElement& e : elements_) {
    switch (e.type_) {
      case FormDataElement::kData:
        size += e.data_.size();
        break;
      case FormDataElement::kEncodedFile:
        size += e.file_length_ - e.file_start_;
        break;
      case FormDataElement::kEncodedBlob:
        if (e.optional_blob_data_handle_)
          size += e.optional_blob_data_handle_->size();
        break;
      case FormDataElement::kDataPipe:
        // We can get the size but it'd be async. Data pipe elements exist only
        // in EncodedFormData instances that were filled from the content side
        // using the WebHTTPBody interface, and generally represent blobs.
        // Since for actual kEncodedBlob elements we ignore their size as well
        // if the element was created through WebHTTPBody (which never sets
        // optional_blob_data_handle), we'll ignore the size of these elements
        // as well.
        break;
    }
  }
  return size;
}

bool EncodedFormData::IsSafeToSendToAnotherThread() const {
  return HasOneRef();
}

}  // namespace blink
