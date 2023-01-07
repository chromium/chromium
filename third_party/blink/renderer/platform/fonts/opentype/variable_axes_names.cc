// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/opentype/variable_axes_names.h"

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
  std::unique_ptr<hb_ot_var_axis_info_t[]> axes =
      std::make_unique<hb_ot_var_axis_info_t[]>(axes_count);
  hb_ot_var_get_axis_infos(face.get(), 0, &axes_count, axes.get());

  for (unsigned i = 0; i < axes_count; i++) {
    VariationAxis axis;

    // HB_LANGUAGE_INVALID fetches the default English string according
    // to HarfBuzz documentation. If the buffer is nullptr, it returns
    // the length of the name without writing to the buffer.
    unsigned name_length = hb_ot_name_get_utf16(
        face.get(), axes[i].name_id, HB_LANGUAGE_INVALID, nullptr, nullptr);

    axis.name = "";
    if (name_length) {
      unsigned buffer_length = name_length + 1;
      std::unique_ptr<char16_t[]> buffer =
          std::make_unique<char16_t[]>(buffer_length);
      hb_ot_name_get_utf16(face.get(), axes[i].name_id, HB_LANGUAGE_INVALID,
                           &buffer_length,
                           reinterpret_cast<uint16_t*>(buffer.get()));
      axis.name = String(buffer.get());
    }

    std::array<uint8_t, 4> tag = {HB_UNTAG(axes[i].tag)};

    axis.tag = String(reinterpret_cast<const char*>(tag.data()), tag.size());
    axis.minValue = axes[i].min_value;
    axis.maxValue = axes[i].max_value;
    axis.defaultValue = axes[i].default_value;

    output.push_back(axis);
  }
  return output;
}

}  // namespace blink
