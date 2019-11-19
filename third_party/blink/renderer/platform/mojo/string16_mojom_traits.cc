// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"

#include <cstring>

#include "base/containers/span.h"
#include "base/strings/latin1_string_conversions.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

MaybeOwnedString16::MaybeOwnedString16(base::string16 owned_storage)
    : owned_storage_(owned_storage),
      unowned_(base::make_span(
          reinterpret_cast<const uint16_t*>(owned_storage_.data()),
          owned_storage_.size())) {}

MaybeOwnedString16::MaybeOwnedString16(base::span<const uint16_t> unowned)
    : unowned_(unowned) {}

MaybeOwnedString16::~MaybeOwnedString16() = default;

// static
MaybeOwnedString16 StructTraits<mojo_base::mojom::String16DataView,
                                WTF::String>::data(const WTF::String& input) {
  if (input.Is8Bit()) {
    return MaybeOwnedString16(base::Latin1OrUTF16ToUTF16(
        input.length(), input.Characters8(), nullptr));
  }
  return MaybeOwnedString16(base::make_span(
      reinterpret_cast<const uint16_t*>(input.Characters16()), input.length()));
}

// static
bool StructTraits<mojo_base::mojom::String16DataView, WTF::String>::Read(
    mojo_base::mojom::String16DataView data,
    WTF::String* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  if (view.size() > std::numeric_limits<uint32_t>::max())
    return false;
  *out = WTF::String(reinterpret_cast<const UChar*>(view.data()),
                     static_cast<uint32_t>(view.size()));
  return true;
}

// static
mojo_base::BigBuffer StructTraits<mojo_base::mojom::BigString16DataView,
                                  WTF::String>::data(const WTF::String& input) {
  if (input.Is8Bit()) {
    base::string16 input16(input.Characters8(),
                           input.Characters8() + input.length());
    return mojo_base::BigBuffer(base::as_bytes(base::make_span(input16)));
  }

  return mojo_base::BigBuffer(base::as_bytes(input.Span16()));
}

// static
bool StructTraits<mojo_base::mojom::BigString16DataView, WTF::String>::Read(
    mojo_base::mojom::BigString16DataView data,
    WTF::String* out) {
  mojo_base::BigBuffer buffer;
  if (!data.ReadData(&buffer))
    return false;
  size_t size = buffer.size();
  if (size % sizeof(UChar))
    return false;

  size /= sizeof(UChar);
  if (size > std::numeric_limits<uint32_t>::max())
    return false;

  // An empty |mojo_base::BigBuffer| may have a null |data()| if empty.
  if (!size) {
    *out = g_empty_string;
  } else {
    *out = WTF::String(reinterpret_cast<const UChar*>(buffer.data()),
                       static_cast<uint32_t>(size));
  }

  return true;
}

}  // namespace mojo
