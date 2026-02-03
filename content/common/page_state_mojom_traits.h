// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PAGE_STATE_MOJOM_TRAITS_H_
#define CONTENT_COMMON_PAGE_STATE_MOJOM_TRAITS_H_

#include <string>
#include <utility>

#include "content/common/navigation_client.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/page_state/page_state.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::PageStateDataView, blink::PageState> {
  static const std::string& data(const blink::PageState& state) {
    return state.ToEncodedData();
  }

  static bool Read(content::mojom::PageStateDataView in,
                   blink::PageState* out) {
    std::string data;
    if (!in.ReadData(&data)) {
      return false;
    }
    *out = blink::PageState::CreateFromEncodedData(std::move(data));
    return true;
  }
};

}  // namespace mojo

#endif  // CONTENT_COMMON_PAGE_STATE_MOJOM_TRAITS_H_
