// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_BASE_H_
#define SKIA_EXT_SKIA_UTILS_BASE_H_

#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

namespace base {
class Pickle;
class PickleIterator;
}

class SkBitmap;
class SkFlattenable;

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

// Converts an SkBitmap to an Opaque or Premul N32 SkBitmap. If the input is in
// the right format (N32 Opaque or Premul) already, points |out| directly at
// |in|. |out| may or may not be GPU-backed.
//
// If unsuccessful, returns false, but |out| may be modified.
//
// This should be called as early as possible at IPC endpoints from
// less-privileged contexts (e.g. on a message from the renderer process) if the
// code handling the SkBitmap wants to work with an N32 type, rather than
// delaying this conversion until a later time.
SK_API bool SkBitmapToN32OpaqueOrPremul(const SkBitmap& in, SkBitmap* out);

}  // namespace skia

#endif  // SKIA_EXT_SKIA_UTILS_BASE_H_
