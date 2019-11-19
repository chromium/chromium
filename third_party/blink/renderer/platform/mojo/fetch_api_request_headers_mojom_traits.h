// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FETCH_API_REQUEST_HEADERS_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FETCH_API_REQUEST_HEADERS_MOJOM_TRAITS_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

template <>
struct StructTraits<
    blink::mojom::blink::FetchAPIRequestHeaders::DataView,
    WTF::HashMap<WTF::String, WTF::String, WTF::CaseFoldingHash>> {
  static WTF::HashMap<WTF::String, WTF::String> headers(
      const WTF::HashMap<WTF::String, WTF::String, WTF::CaseFoldingHash>&
          input) {
    WTF::HashMap<WTF::String, WTF::String> map;
    for (const auto& tuple : input)
      map.insert(tuple.key, tuple.value);
    return map;
  }

  static bool Read(
      blink::mojom::blink::FetchAPIRequestHeaders::DataView in,
      WTF::HashMap<WTF::String, WTF::String, WTF::CaseFoldingHash>* out) {
    WTF::HashMap<WTF::String, WTF::String> in_headers;
    if (!in.ReadHeaders(&in_headers))
      return false;
    for (const auto& tuple : in_headers)
      out->insert(tuple.key, tuple.value);
    return true;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_FETCH_API_REQUEST_HEADERS_MOJOM_TRAITS_H_
