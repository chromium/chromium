// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/encoded_form_data_mojom_traits.h"

#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data_element_mojom_traits.h"
#include "third_party/blink/renderer/platform/network/form_data_encoder.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                  scoped_refptr<blink::EncodedFormData>>::
    Read(blink::mojom::FetchAPIRequestBodyDataView in,
         scoped_refptr<blink::EncodedFormData>* out) {
  *out = blink::EncodedFormData::Create();
  if (!in.ReadElements(&((*out)->elements_))) {
    return false;
  }
  (*out)->identifier_ = in.identifier();
  (*out)->contains_password_data_ = in.contains_sensitive_info();
  (*out)->SetBoundary(blink::FormDataEncoder::GenerateUniqueBoundaryString());

  return true;
}

}  // namespace mojo
