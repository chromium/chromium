#include "third_party/blink/renderer/core/loader/beacon_data.h"

#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/html/forms/form_data.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/url/url_search_params.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"

namespace blink {

BeaconString::BeaconString(const String& data)
    : data_(data), content_type_("text/plain;charset=UTF-8") {}

uint64_t BeaconString::size() const {
  return data_.CharactersSizeInBytes();
}

scoped_refptr<EncodedFormData> BeaconString::GetEncodedFormData() const {
  return EncodedFormData::Create(data_.Utf8());
}

void BeaconString::Serialize(ResourceRequest& request) const {
  request.SetHttpBody(GetEncodedFormData());
  if (!data_.IsNull()) {
    request.SetHTTPContentType(GetContentType());
  }
}

BeaconBlob::BeaconBlob(Blob* data) : data_(data) {
  const String& blob_type = data_->type();
  if (!blob_type.empty() && ParsedContentType(blob_type).IsValid())
    content_type_ = AtomicString(blob_type);
}

uint64_t BeaconBlob::size() const {
  return data_->size();
}

scoped_refptr<EncodedFormData> BeaconBlob::GetEncodedFormData() const {
  DCHECK(data_);

  scoped_refptr<EncodedFormData> entity_body = EncodedFormData::Create();
  if (data_->HasBackingFile()) {
    entity_body->AppendFile(To<File>(data_)->GetPath(),
                            To<File>(data_)->LastModifiedTime());
  } else {
    entity_body->AppendBlob(data_->Uuid(), data_->GetBlobDataHandle());
  }

  return entity_body;
}

void BeaconBlob::Serialize(ResourceRequest& request) const {
  request.SetHttpBody(GetEncodedFormData());

  if (!GetContentType().empty()) {
    if (!cors::IsCorsSafelistedContentType(GetContentType())) {
      request.SetMode(network::mojom::blink::RequestMode::kCors);
    }
    request.SetHTTPContentType(GetContentType());
  }
}

BeaconDOMArrayBufferView::BeaconDOMArrayBufferView(DOMArrayBufferView* data)
    : data_(data) {
  CHECK(base::CheckedNumeric<wtf_size_t>(data->byteLength()).IsValid())
      << "EncodedFormData::Create cannot deal with huge ArrayBuffers.";
}

uint64_t BeaconDOMArrayBufferView::size() const {
  return data_->byteLength();
}

scoped_refptr<EncodedFormData> BeaconDOMArrayBufferView::GetEncodedFormData()
    const {
  DCHECK(data_);

  return EncodedFormData::Create(
      data_->BaseAddress(),
      base::checked_cast<wtf_size_t>(data_->byteLength()));
}

void BeaconDOMArrayBufferView::Serialize(ResourceRequest& request) const {
  request.SetHttpBody(GetEncodedFormData());
}

BeaconDOMArrayBuffer::BeaconDOMArrayBuffer(DOMArrayBuffer* data) : data_(data) {
  CHECK(base::CheckedNumeric<wtf_size_t>(data->ByteLength()).IsValid())
      << "EncodedFormData::Create cannot deal with huge ArrayBuffers.";
}

uint64_t BeaconDOMArrayBuffer::size() const {
  return data_->ByteLength();
}

scoped_refptr<EncodedFormData> BeaconDOMArrayBuffer::GetEncodedFormData()
    const {
  DCHECK(data_);

  return EncodedFormData::Create(
      data_->Data(), base::checked_cast<wtf_size_t>(data_->ByteLength()));
}

void BeaconDOMArrayBuffer::Serialize(ResourceRequest& request) const {
  request.SetHttpBody(GetEncodedFormData());
}

BeaconURLSearchParams::BeaconURLSearchParams(URLSearchParams* data)
    : data_(data),
      content_type_("application/x-www-form-urlencoded;charset=UTF-8") {}

uint64_t BeaconURLSearchParams::size() const {
  return data_->toString().CharactersSizeInBytes();
}

scoped_refptr<EncodedFormData> BeaconURLSearchParams::GetEncodedFormData()
    const {
  DCHECK(data_);

  return data_->ToEncodedFormData();
}

void BeaconURLSearchParams::Serialize(ResourceRequest& request) const {
  DCHECK(data_);

  request.SetHttpBody(GetEncodedFormData());
  request.SetHTTPContentType(GetContentType());
}

BeaconFormData::BeaconFormData(FormData* data)
    : data_(data),
      entity_body_(data_->EncodeMultiPartFormData()),
      content_type_(String("multipart/form-data; boundary=") +
                    entity_body_->Boundary().data()) {}

uint64_t BeaconFormData::size() const {
  return entity_body_->SizeInBytes();
}

scoped_refptr<EncodedFormData> BeaconFormData::GetEncodedFormData() const {
  return entity_body_;
}

void BeaconFormData::Serialize(ResourceRequest& request) const {
  request.SetHttpBody(GetEncodedFormData());
  request.SetHTTPContentType(GetContentType());
}

}  // namespace blink
