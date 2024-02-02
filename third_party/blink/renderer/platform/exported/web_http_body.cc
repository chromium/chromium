/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_http_body.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/data_pipe_getter.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"
#include "third_party/blink/renderer/platform/network/wrapped_data_pipe_getter.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

void WebHTTPBody::Initialize() {
  private_ = EncodedFormData::Create();
}

void WebHTTPBody::Reset() {
  private_ = nullptr;
}

void WebHTTPBody::Assign(const WebHTTPBody& other) {
  private_ = other.private_;
}

size_t WebHTTPBody::ElementCount() const {
  DCHECK(!IsNull());
  return private_->Elements().size();
}

bool WebHTTPBody::ElementAt(size_t index, Element& result) const {
  DCHECK(!IsNull());

  if (index >= private_->Elements().size())
    return false;

  const FormDataElement& element =
      private_->Elements()[static_cast<wtf_size_t>(index)];

  result.data.Reset();
  result.file_path.Reset();
  result.file_start = 0;
  result.file_length = 0;
  result.modification_time = std::nullopt;

  switch (element.type_) {
    case FormDataElement::kData:
      result.type = HTTPBodyElementType::kTypeData;
      result.data.Assign(element.data_.data(), element.data_.size());
      break;
    case FormDataElement::kEncodedFile:
      result.type = HTTPBodyElementType::kTypeFile;
      result.file_path = element.filename_;
      result.file_start = element.file_start_;
      result.file_length = element.file_length_;
      result.modification_time = element.expected_file_modification_time_;
      break;
    case FormDataElement::kEncodedBlob:
      result.type = HTTPBodyElementType::kTypeBlob;
      result.blob_length = std::numeric_limits<uint64_t>::max();
      result.optional_blob =
          element.optional_blob_data_handle_->CloneBlobRemote();
      result.blob_length = element.optional_blob_data_handle_->size();
      break;
    case FormDataElement::kDataPipe:
      result.type = HTTPBodyElementType::kTypeDataPipe;
      mojo::PendingRemote<network::mojom::blink::DataPipeGetter>
          data_pipe_getter;
      element.data_pipe_getter_->GetDataPipeGetter()->Clone(
          data_pipe_getter.InitWithNewPipeAndPassReceiver());
      result.data_pipe_getter = std::move(data_pipe_getter);
      break;
  }

  return true;
}

void WebHTTPBody::AppendData(const WebData& data) {
  EnsureMutable();
  // FIXME: FormDataElement::m_data should be a SharedBuffer<char>.  Then we
  // could avoid this buffer copy.
  data.ForEachSegment([this](const char* segment, size_t segment_size,
                             size_t segment_offset) {
    private_->AppendData(segment, base::checked_cast<wtf_size_t>(segment_size));
    return true;
  });
}

void WebHTTPBody::AppendFileRange(
    const WebString& file_path,
    int64_t file_start,
    int64_t file_length,
    const std::optional<base::Time>& modification_time) {
  EnsureMutable();
  private_->AppendFileRange(file_path, file_start, file_length,
                            modification_time);
}

void WebHTTPBody::AppendBlob(const WebString& uuid) {
  EnsureMutable();
  private_->AppendBlob(uuid, nullptr);
}

void WebHTTPBody::AppendDataPipe(
    CrossVariantMojoRemote<network::mojom::DataPipeGetterInterfaceBase>
        data_pipe_getter) {
  EnsureMutable();

  auto wrapped =
      base::MakeRefCounted<WrappedDataPipeGetter>(std::move(data_pipe_getter));
  private_->AppendDataPipe(std::move(wrapped));
}

int64_t WebHTTPBody::Identifier() const {
  DCHECK(!IsNull());
  return private_->Identifier();
}

void WebHTTPBody::SetIdentifier(int64_t identifier) {
  EnsureMutable();
  return private_->SetIdentifier(identifier);
}

void WebHTTPBody::SetUniqueBoundary() {
  EnsureMutable();
  private_->SetBoundary(FormDataEncoder::GenerateUniqueBoundaryString());
}

bool WebHTTPBody::ContainsPasswordData() const {
  return private_->ContainsPasswordData();
}

void WebHTTPBody::SetContainsPasswordData(bool contains_password_data) {
  private_->SetContainsPasswordData(contains_password_data);
}

WebHTTPBody::WebHTTPBody(scoped_refptr<EncodedFormData> data)
    : private_(std::move(data)) {}

WebHTTPBody& WebHTTPBody::operator=(scoped_refptr<EncodedFormData> data) {
  private_ = std::move(data);
  return *this;
}

WebHTTPBody::operator scoped_refptr<EncodedFormData>() const {
  return private_.Get();
}

void WebHTTPBody::EnsureMutable() {
  DCHECK(!IsNull());
  if (!private_->HasOneRef())
    private_ = private_->Copy();
}

}  // namespace blink
