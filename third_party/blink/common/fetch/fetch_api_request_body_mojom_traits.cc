// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fetch/fetch_api_request_body_mojom_traits.h"

#include "services/network/public/cpp/url_request_mojom_traits.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace mojo {

bool StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                  scoped_refptr<network::ResourceRequestBody>>::
    Read(blink::mojom::FetchAPIRequestBodyDataView data,
         scoped_refptr<network::ResourceRequestBody>* out) {
  auto body = base::MakeRefCounted<network::ResourceRequestBody>();
  if (!data.ReadElements(&(body->elements_)))
    return false;
  body->set_identifier(data.identifier());
  body->set_contains_sensitive_info(data.contains_sensitive_info());
  *out = std::move(body);
  return true;
}

bool StructTraits<
    blink::mojom::FetchAPIDataElementDataView,
    network::DataElement>::Read(blink::mojom::FetchAPIDataElementDataView data,
                                network::DataElement* out) {
  if (!data.ReadPath(&out->path_) ||
      !data.ReadExpectedModificationTime(&out->expected_modification_time_)) {
    return false;
  }

  if (data.type() == network::mojom::DataElementType::kBytes) {
    if (!data.ReadBuf(&out->buf_))
      return false;
  }
  out->type_ = data.type();
  out->data_pipe_getter_ = data.TakeDataPipeGetter<
      mojo::PendingRemote<network::mojom::DataPipeGetter>>();
  out->chunked_data_pipe_getter_ = data.TakeChunkedDataPipeGetter<
      mojo::PendingRemote<network::mojom::ChunkedDataPipeGetter>>();
  out->offset_ = data.offset();
  out->length_ = data.length();
  return true;
}

}  // namespace mojo
