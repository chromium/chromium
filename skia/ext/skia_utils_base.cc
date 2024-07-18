// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/skia_utils_base.h"

#include <stdint.h>

#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/modules/skcms/skcms.h"

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
  if (SkColorGetA(color) == 0xFF) {
    return base::StringPrintf("#%02X%02X%02X", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color));
  } else {
    return base::StringPrintf("#%02X%02X%02X%02X", SkColorGetR(color),
                              SkColorGetG(color), SkColorGetB(color),
                              SkColorGetA(color));
  }
}

std::string SkColorSpaceToString(const SkColorSpace* cs) {
  if (!cs) {
    return "null";
  }
  skcms_TransferFunction trfn;
  skcms_Matrix3x3 to_xyzd50;
  cs->toXYZD50(&to_xyzd50);
  cs->transferFn(&trfn);
  return base::StringPrintf("{trfn:%s, toXYZD50:%s}",
                            SkcmsTransferFunctionToString(trfn).c_str(),
                            SkcmsMatrix3x3ToString(to_xyzd50).c_str());
}

std::string SkcmsTransferFunctionToString(const skcms_TransferFunction& fn) {
  return base::StringPrintf(
      "{%1.4f*x + %1.4f if abs(x) < %1.4f else "
      "sign(x)*((%1.4f*abs(x) + %1.4f)**%1.4f + %1.4f)}",
      fn.c, fn.f, fn.d, fn.a, fn.b, fn.g, fn.e);
}

std::string SkcmsMatrix3x3ToString(const skcms_Matrix3x3& m) {
  return base::StringPrintf(
      "{rows:[[%1.4f,%1.4f,%1.4f],[%1.4f,%1.4f,%1.4f],[%1.4f,%1.4f,%1.4f]]}",
      m.vals[0][0], m.vals[0][1], m.vals[0][2], m.vals[1][0], m.vals[1][1],
      m.vals[1][2], m.vals[2][0], m.vals[2][1], m.vals[2][2]);
}

}  // namespace skia
