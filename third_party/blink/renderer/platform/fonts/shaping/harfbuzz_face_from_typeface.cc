// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face_from_typeface.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/skia/include/core/SkStream.h"

namespace {
static void DeleteTypefaceStream(void* stream_asset_ptr) {
  SkStreamAsset* stream_asset =
      reinterpret_cast<SkStreamAsset*>(stream_asset_ptr);
  delete stream_asset;
}
}  // namespace

namespace blink {
hb::unique_ptr<hb_face_t> HbFaceFromSkTypeface(sk_sp<SkTypeface> typeface) {
  hb::unique_ptr<hb_face_t> return_face(nullptr);
  int ttc_index = 0;

  // Have openStream() write the ttc index of this typeface within the stream to
  // the ttc_index parameter, so that we can check it below against the count of
  // faces within the buffer, as HarfBuzz counts it.
  std::unique_ptr<SkStreamAsset> tf_stream(typeface->openStream(&ttc_index));
  if (tf_stream && tf_stream->getMemoryBase()) {
    const void* tf_memory = tf_stream->getMemoryBase();
    size_t tf_size = tf_stream->getLength();
    hb::unique_ptr<hb_blob_t> face_blob(hb_blob_create(
        reinterpret_cast<const char*>(tf_memory),
        base::checked_cast<unsigned int>(tf_size), HB_MEMORY_MODE_READONLY,
        tf_stream.release(), DeleteTypefaceStream));
    // hb_face_create always succeeds.
    // Use hb_face_count to retrieve the number of recognized faces in the blob.
    // hb_face_create_for_tables may still create a working hb_face.
    // See https://github.com/harfbuzz/harfbuzz/issues/248 .
    unsigned int num_hb_faces = hb_face_count(face_blob.get());
    if (0 < num_hb_faces && static_cast<unsigned>(ttc_index) < num_hb_faces) {
      return_face =
          hb::unique_ptr<hb_face_t>(hb_face_create(face_blob.get(), ttc_index));
    }
  }
  return return_face;
}
}  // namespace blink
