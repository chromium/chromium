/*
 *  Copyright (C) 2006 George Staikos <staikos@kde.org>
 *  Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UNICODE_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UNICODE_STRING_H_

#include <unicode/stringoptions.h>
#include <unicode/ustring.h>

namespace WTF {
namespace unicode {

inline int FoldCase(UChar* result,
                    int result_length,
                    const UChar* src,
                    int src_length,
                    bool* error) {
  UErrorCode status = U_ZERO_ERROR;
  int real_length = u_strFoldCase(result, result_length, src, src_length,
                                  U_FOLD_CASE_DEFAULT, &status);
  *error = !U_SUCCESS(status);
  return real_length;
}

inline int Umemcasecmp(const UChar* a, const UChar* b, int len) {
  return u_memcasecmp(a, b, len, U_FOLD_CASE_DEFAULT);
}

}  // namespace unicode
}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UNICODE_STRING_H_
