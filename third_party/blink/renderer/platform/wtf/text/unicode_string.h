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
#include <unicode/unistr.h>
#include <unicode/ustring.h>

#include "base/containers/span.h"

#if U_ICU_VERSION_MAJOR_NUM >= 59
#include <unicode/char16ptr.h>
#endif

namespace blink::unicode {

inline std::optional<size_t> FoldCase(base::span<const UChar> src,
                                      base::span<UChar> result) {
  UErrorCode status = U_ZERO_ERROR;
  int real_length = u_strFoldCase(
      result.data(), base::checked_cast<int>(result.size()), src.data(),
      base::checked_cast<int>(src.size()), U_FOLD_CASE_DEFAULT, &status);
  if (U_SUCCESS(status) || status == U_BUFFER_OVERFLOW_ERROR) {
    return real_length;
  }
  return std::nullopt;
}

inline int Umemcasecmp(const UChar* a, const UChar* b, int len) {
  return u_memcasecmp(a, b, len, U_FOLD_CASE_DEFAULT);
}

inline base::span<const UChar> ToSpan(const icu::UnicodeString& ustring) {
  size_t size = static_cast<size_t>(ustring.length());
  // SAFETY: ICU ensures ustring.length() is valid for ustring.getBuffer().
#if U_ICU_VERSION_MAJOR_NUM >= 59
  return UNSAFE_BUFFERS(base::span(icu::toUCharPtr(ustring.getBuffer()), size));
#else
  return UNSAFE_BUFFERS(base::span(ustring.getBuffer(), size));
#endif
}

}  // namespace blink::unicode

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_UNICODE_STRING_H_
