// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_BASE_H_
#define SKIA_EXT_SKIA_UTILS_BASE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace base {
class Pickle;
class PickleIterator;
}

class SkBitmap;
class SkFlattenable;
class SkColorSpace;
struct skcms_Matrix3x3;
struct skcms_TransferFunction;

namespace skia {

// Return true if the pickle/iterator contains a string. If so, and if str
// is not null, copy that string into str.
SK_API bool ReadSkString(base::PickleIterator* iter, SkString* str);

// Return true if the pickle/iterator contains a FontIdentity. If so, and if
// identity is not null, copy it into identity.
SK_API bool ReadSkFontIdentity(base::PickleIterator* iter,
                               SkFontConfigInterface::FontIdentity* identity);

// Return true if the pickle/iterator contains a SkFontStyle. If so, and if
// style is not null, copy it into style.
SK_API bool ReadSkFontStyle(base::PickleIterator* iter, SkFontStyle* style);

// Writes str into the request pickle.
SK_API void WriteSkString(base::Pickle* pickle, const SkString& str);

// Writes identity into the request pickle.
SK_API void WriteSkFontIdentity(
    base::Pickle* pickle,
    const SkFontConfigInterface::FontIdentity& identity);

// Writes style into the request pickle.
SK_API void WriteSkFontStyle(base::Pickle* pickle, SkFontStyle style);

// Converts an SkBitmap to an Opaque or Premul N32 SkBitmap with stride matching
// the width of each row. If the input is has the right format (N32 Opaque or
// Premul) without stride padding already, this assigns `in` to `out`, sharing
// the backing pixels. `out` may or may not be GPU-backed.
//
// If unsuccessful, returns false, but |out| may be modified.
//
// This should be called as early as possible at IPC endpoints from
// less-privileged contexts (e.g. on a message from the renderer process) if the
// code handling the SkBitmap wants to work with an N32 type, rather than
// delaying this conversion until a later time.
SK_API bool SkBitmapToN32OpaqueOrPremul(const SkBitmap& in, SkBitmap* out);

// Returns hex string representation for the |color| in "#FFFFFF" format.
SK_API std::string SkColorToHexString(SkColor color);

// Return a string representation of an SkColorSpace. Accepts nullptr.
SK_API std::string SkColorSpaceToString(const SkColorSpace* cs);

// Return string representation of skcms matrix and transfer functions.
SK_API std::string SkcmsMatrix3x3ToString(const skcms_Matrix3x3& m);
SK_API std::string SkcmsTransferFunctionToString(
    const skcms_TransferFunction& f);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_BASE_H_
