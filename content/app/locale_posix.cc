// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/locale_posix.h"

#include <locale.h>

namespace content {

void InitializeCLibraryLocale() {
  // Set C library locale to make sure CommandLine can parse
  // argument values in the correct encoding and to make sure
  // generated file names (think downloads) are in the file system's
  // encoding.
  if (!setlocale(LC_ALL, "")) {
    // At least one category names a locale that isn't installed, so nothing
    // was changed. Set the categories one by one so that the valid ones still
    // take effect; LC_NUMERIC is set to "C" below anyway.
    // clang-format off
    static constexpr int kCategories[] = {
      LC_CTYPE,
      LC_COLLATE,
      LC_MONETARY,
      LC_MESSAGES,
      LC_TIME,
#if defined(LC_PAPER)
      LC_PAPER,
#endif
#if defined(LC_NAME)
      LC_NAME,
#endif
#if defined(LC_ADDRESS)
      LC_ADDRESS,
#endif
#if defined(LC_TELEPHONE)
      LC_TELEPHONE,
#endif
#if defined(LC_MEASUREMENT)
      LC_MEASUREMENT,
#endif
#if defined(LC_IDENTIFICATION)
      LC_IDENTIFICATION,
#endif
    };
    // clang-format on
    bool ctype_set = false;
    for (int category : kCategories) {
      const bool set = setlocale(category, "") != nullptr;
      if (category == LC_CTYPE) {
        ctype_set = set;
      }
    }
    if (!ctype_set) {
      // In the "C" locale, multibyte conversions fail on any non-ASCII
      // character, which breaks non-ASCII file names. UTF-8 is a better guess
      // for the file system encoding, and is no worse for others.
      setlocale(LC_CTYPE, "C.UTF-8");
    }
  }

  // For numbers the C library's locale sensitive conversion from number to
  // string is never wanted because the only thing it changes is the decimal
  // separator which is not good enough for the UI and can be harmful
  // elsewhere. User interface number conversions need to go through ICU.
  // Other conversions need to be locale insensitive so force the number locale
  // back to the default, "C", locale.
  setlocale(LC_NUMERIC, "C");
}

}  // namespace content
