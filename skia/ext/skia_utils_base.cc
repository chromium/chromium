// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_base.h"

#include <stdint.h>

#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSerialProcs.h"

namespace skia {

bool ReadSkString(base::PickleIterator* iter, SkString* str) {
  size_t reply_length;
  const char* reply_text;

  if (!iter->ReadData(&reply_text, &reply_length))
    return false;

  if (str)
    str->set(reply_text, reply_length);
  return true;
}

bool ReadSkFontIdentity(base::PickleIterator* iter,
                        SkFontConfigInterface::FontIdentity* identity) {
  uint32_t reply_id;
  uint32_t reply_ttcIndex;
  size_t reply_length;
  const char* reply_text;

  if (!iter->ReadUInt32(&reply_id) ||
      !iter->ReadUInt32(&reply_ttcIndex) ||
      !iter->ReadData(&reply_text, &reply_length))
    return false;

  if (identity) {
    identity->fID = reply_id;
    identity->fTTCIndex = reply_ttcIndex;
    identity->fString.set(reply_text, reply_length);
  }
  return true;
}

bool ReadSkFontStyle(base::PickleIterator* iter, SkFontStyle* style) {
  uint16_t reply_weight;
  uint16_t reply_width;
  uint16_t reply_slant;

  if (!iter->ReadUInt16(&reply_weight) ||
      !iter->ReadUInt16(&reply_width) ||
      !iter->ReadUInt16(&reply_slant))
    return false;

  if (style) {
    *style = SkFontStyle(reply_weight,
                         reply_width,
                         static_cast<SkFontStyle::Slant>(reply_slant));
  }
  return true;
}

void WriteSkString(base::Pickle* pickle, const SkString& str) {
  pickle->WriteData(str.c_str(), str.size());
}

void WriteSkFontIdentity(base::Pickle* pickle,
                         const SkFontConfigInterface::FontIdentity& identity) {
  pickle->WriteUInt32(identity.fID);
  pickle->WriteUInt32(identity.fTTCIndex);
  WriteSkString(pickle, identity.fString);
}

void WriteSkFontStyle(base::Pickle* pickle, SkFontStyle style) {
  pickle->WriteUInt16(style.weight());
  pickle->WriteUInt16(style.width());
  pickle->WriteUInt16(style.slant());
}

bool SkBitmapToN32OpaqueOrPremul(const SkBitmap& in, SkBitmap* out) {
  DCHECK(out);
  if (in.colorType() == kUnknown_SkColorType &&
      in.alphaType() == kUnknown_SkAlphaType && in.empty() && in.isNull()) {
    // Default-initialized bitmaps convert to the same.
    *out = SkBitmap();
    return true;
  }
  const SkImageInfo& info = in.info();
  const bool stride_matches_width = in.rowBytes() == info.minRowBytes();
  if (stride_matches_width && info.colorType() == kN32_SkColorType &&
      (info.alphaType() == kPremul_SkAlphaType ||
       info.alphaType() == kOpaque_SkAlphaType)) {
    // Shallow copy if the data is already in the right format.
    *out = in;
    return true;
  }

  SkImageInfo new_info =
      info.makeColorType(kN32_SkColorType)
          .makeAlphaType(info.alphaType() == kOpaque_SkAlphaType
                             ? kOpaque_SkAlphaType
                             : kPremul_SkAlphaType);
  if (!out->tryAllocPixels(new_info, 0)) {
    return false;
  }
  if (!in.readPixels(out->pixmap())) {
    return false;
  }
  return true;
}

std::string SkColorToHexString(SkColor color) {
  return base::StringPrintf("#%02X%02X%02X", SkColorGetR(color),
                            SkColorGetG(color), SkColorGetB(color));
}
}  // namespace skia
