// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_LOCALE_POSIX_H_
#define CONTENT_APP_LOCALE_POSIX_H_

namespace content {

// Sets the C library locale from the environment (LC_ALL, LC_*, LANG), except
// LC_NUMERIC, which is set to "C".
//
// setlocale(LC_ALL, "") fails as a whole, leaving the process in the "C"
// locale, when any single category names a locale that isn't installed (e.g.
// LC_TIME=fi_FI.UTF-8 without that locale). In that case each category is set
// on its own so that the valid ones still apply. If LC_CTYPE itself can't be
// set, "C.UTF-8" is used for it, so that multibyte conversions of file names
// (mbstowcs() and friends) don't drop non-ASCII characters.
void InitializeCLibraryLocale();

}  // namespace content

#endif  // CONTENT_APP_LOCALE_POSIX_H_
