/**
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <CoreFoundation/CFString.h>

namespace WTF {

String::String(NSString* str) {
  if (!str)
    return;

  CFIndex size = CFStringGetLength(reinterpret_cast<CFStringRef>(str));
  if (size == 0)
    impl_ = StringImpl::empty_;
  else {
    Vector<LChar, 1024> lchar_buffer(size);
    CFIndex used_buf_len;
    CFIndex convertedsize =
        CFStringGetBytes(reinterpret_cast<CFStringRef>(str),
                         CFRangeMake(0, size), kCFStringEncodingISOLatin1, 0,
                         false, lchar_buffer.data(), size, &used_buf_len);
    if ((convertedsize == size) && (used_buf_len == size)) {
      impl_ = StringImpl::Create(lchar_buffer.data(), size);
      return;
    }

    Vector<UChar, 1024> uchar_buffer(size);
    CFStringGetCharacters(reinterpret_cast<CFStringRef>(str),
                          CFRangeMake(0, size),
                          reinterpret_cast<UniChar*>(uchar_buffer.data()));
    impl_ = StringImpl::Create(uchar_buffer.data(), size);
  }
}

}  // namespace WTF
