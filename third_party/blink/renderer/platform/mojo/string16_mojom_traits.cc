// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mojo/string16_mojom_traits.h"

#include <cstring>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"

namespace mojo {

MaybeOwnedString16::MaybeOwnedString16(std::u16string owned_storage)
    : owned_storage_(owned_storage),
      UNSAFE_TODO(
          unowned_(reinterpret_cast<const uint16_t*>(owned_storage_.data()),
                   owned_storage_.size())) {}

MaybeOwnedString16::MaybeOwnedString16(base::span<const uint16_t> unowned)
    : unowned_(unowned) {}

MaybeOwnedString16::~MaybeOwnedString16() = default;

// static
MaybeOwnedString16
StructTraits<mojo_base::mojom::String16DataView, blink::String>::data(
    const blink::String& input) {
  if (input.Is8Bit()) {
    auto latin1 = input.Span8();
    return MaybeOwnedString16(std::u16string(latin1.begin(), latin1.end()));
  }
  return MaybeOwnedString16(input.SpanUint16());
}

// static
bool StructTraits<mojo_base::mojom::String16DataView, blink::String>::Read(
    mojo_base::mojom::String16DataView data,
    blink::String* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  if (view.size() > std::numeric_limits<uint32_t>::max())
    return false;
  *out = blink::String(UNSAFE_TODO(
      base::span(reinterpret_cast<const UChar*>(view.data()), view.size())));
  return true;
}

// static
mojo_base::BigBuffer
StructTraits<mojo_base::mojom::BigString16DataView, blink::String>::data(
    const blink::String& input) {
  if (input.Is8Bit()) {
    auto latin1 = input.Span8();
    std::u16string input16(latin1.begin(), latin1.end());
    return mojo_base::BigBuffer(base::as_byte_span(input16));
  }

  return mojo_base::BigBuffer(base::as_bytes(input.Span16()));
}

// static
bool StructTraits<mojo_base::mojom::BigString16DataView, blink::String>::Read(
    mojo_base::mojom::BigString16DataView data,
    blink::String* out) {
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
    *out = blink::g_empty_string;
  } else {
    *out = blink::String(UNSAFE_TODO(
        base::span(reinterpret_cast<const UChar*>(buffer.data()), size)));
  }

  return true;
}

}  // namespace mojo
