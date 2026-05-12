// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/variable_axes_names.h"

#include "base/containers/heap_array.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

// clang-format off
#include <hb.h>
#include <hb-cplusplus.hh>
#include <hb-ot.h>
// clang-format on

namespace blink {

Vector<VariationAxis> VariableAxesNames::GetVariationAxes(
    sk_sp<SkTypeface> typeface) {
  Vector<VariationAxis> output;
  std::unique_ptr<SkStreamAsset> stream = typeface->openStream(nullptr);
  if (!stream)
    return output;
  sk_sp<SkData> sk_data =
      SkData::MakeFromStream(stream.get(), stream->getLength());
  hb::unique_ptr<hb_blob_t> blob(
      hb_blob_create(reinterpret_cast<const char*>(sk_data->bytes()),
                     base::checked_cast<unsigned>(sk_data->size()),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr));
  hb::unique_ptr<hb_face_t> face(hb_face_create(blob.get(), 0));
  unsigned axes_count = hb_ot_var_get_axis_count(face.get());
  auto axes = base::HeapArray<hb_ot_var_axis_info_t>::WithSize(axes_count);
  hb_ot_var_get_axis_infos(face.get(), 0, &axes_count, axes.data());

  for (const hb_ot_var_axis_info_t& hb_axis : axes) {
    VariationAxis axis;

    // HB_LANGUAGE_INVALID fetches the default English string according
    // to HarfBuzz documentation. If the buffer is nullptr, it returns
    // the length of the name without writing to the buffer.
    unsigned name_length = hb_ot_name_get_utf16(
        face.get(), hb_axis.name_id, HB_LANGUAGE_INVALID, nullptr, nullptr);

    axis.name = g_empty_string;
    if (name_length) {
      unsigned buffer_length = name_length + 1;
      auto name_buffer = base::HeapArray<char16_t>::WithSize(buffer_length);
      hb_ot_name_get_utf16(face.get(), hb_axis.name_id, HB_LANGUAGE_INVALID,
                           &buffer_length,
                           reinterpret_cast<uint16_t*>(name_buffer.data()));
      axis.name = String(name_buffer.first(name_length));
    }

    std::array<uint8_t, 4> tag = {HB_UNTAG(hb_axis.tag)};

    axis.tag = String(base::span(tag));
    axis.minValue = hb_axis.min_value;
    axis.maxValue = hb_axis.max_value;
    axis.defaultValue = hb_axis.default_value;

    output.push_back(axis);
  }
  return output;
}

}  // namespace blink
