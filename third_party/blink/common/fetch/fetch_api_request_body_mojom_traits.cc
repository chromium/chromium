// Copyright 2017 The Chromium Authors
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

}  // namespace mojo
