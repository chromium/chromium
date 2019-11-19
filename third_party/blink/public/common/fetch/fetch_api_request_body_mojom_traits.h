// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FETCH_FETCH_API_REQUEST_BODY_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FETCH_FETCH_API_REQUEST_BODY_MOJOM_TRAITS_H_

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-forward.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                    scoped_refptr<network::ResourceRequestBody>> {
  static bool IsNull(const scoped_refptr<network::ResourceRequestBody>& r) {
    return !r;
  }

  static void SetToNull(scoped_refptr<network::ResourceRequestBody>* out) {
    out->reset();
  }

  static const std::vector<network::DataElement>& elements(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return *r->elements();
  }

  static uint64_t identifier(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->identifier_;
  }

  static bool contains_sensitive_info(
      const scoped_refptr<network::ResourceRequestBody>& r) {
    return r->contains_sensitive_info_;
  }

  static bool Read(blink::mojom::FetchAPIRequestBodyDataView data,
                   scoped_refptr<network::ResourceRequestBody>* out);
};

template <>
struct StructTraits<blink::mojom::FetchAPIDataElementDataView,
                    network::DataElement> {
  static const network::mojom::DataElementType& type(
      const network::DataElement& element) {
    return element.type_;
  }
  static std::vector<uint8_t> buf(const network::DataElement& element) {
    if (element.bytes_) {
      return std::vector<uint8_t>(element.bytes_,
                                  element.bytes_ + element.length_);
    }
    return std::move(element.buf_);
  }
  static const base::FilePath& path(const network::DataElement& element) {
    return element.path_;
  }
  static base::File file(const network::DataElement& element) {
    return std::move(const_cast<network::DataElement&>(element).file_);
  }
  static const std::string& blob_uuid(const network::DataElement& element) {
    return element.blob_uuid_;
  }
  static mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter(
      const network::DataElement& element) {
    if (element.type_ != network::mojom::DataElementType::kDataPipe)
      return mojo::NullRemote();
    return element.CloneDataPipeGetter();
  }
  static mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>
  chunked_data_pipe_getter(const network::DataElement& element) {
    if (element.type_ != network::mojom::DataElementType::kChunkedDataPipe)
      return mojo::NullRemote();
    return const_cast<network::DataElement&>(element)
        .ReleaseChunkedDataPipeGetter();
  }
  static uint64_t offset(const network::DataElement& element) {
    return element.offset_;
  }
  static uint64_t length(const network::DataElement& element) {
    return element.length_;
  }
  static const base::Time& expected_modification_time(
      const network::DataElement& element) {
    return element.expected_modification_time_;
  }

  static bool Read(blink::mojom::FetchAPIDataElementDataView data,
                   network::DataElement* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FETCH_FETCH_API_REQUEST_BODY_MOJOM_TRAITS_H_
