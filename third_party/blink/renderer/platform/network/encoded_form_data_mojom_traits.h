// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_MOJOM_TRAITS_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                                    scoped_refptr<blink::EncodedFormData>> {
  static bool IsNull(const scoped_refptr<blink::EncodedFormData>& data) {
    return !data;
  }
  static void SetToNull(scoped_refptr<blink::EncodedFormData>* out) {
    *out = nullptr;
  }
  static const WTF::Vector<blink::FormDataElement>& elements(
      const scoped_refptr<blink::EncodedFormData>& data) {
    return data->elements_;
  }
  static int64_t identifier(const scoped_refptr<blink::EncodedFormData>& data) {
    return data->identifier_;
  }
  static bool contains_sensitive_info(
      const scoped_refptr<blink::EncodedFormData>& data) {
    return data->contains_password_data_;
  }

  static bool Read(blink::mojom::FetchAPIRequestBodyDataView in,
                   scoped_refptr<blink::EncodedFormData>* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_ENCODED_FORM_DATA_MOJOM_TRAITS_H_
