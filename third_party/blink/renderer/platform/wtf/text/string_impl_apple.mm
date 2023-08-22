/*
 * Copyright (C) 2006, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"

namespace WTF {

base::apple::ScopedCFTypeRef<CFStringRef> StringImpl::CreateCFString() {
  return base::apple::ScopedCFTypeRef<CFStringRef>(
      Is8Bit()
          ? CFStringCreateWithBytes(
                kCFAllocatorDefault,
                reinterpret_cast<const UInt8*>(Characters8()), length_,
                kCFStringEncodingISOLatin1, /*isExternalRepresentation=*/false)
          : CFStringCreateWithCharacters(
                kCFAllocatorDefault,
                reinterpret_cast<const UniChar*>(Characters16()), length_));
}

StringImpl::operator NSString*() {
  return base::apple::CFToNSOwnershipCast(CreateCFString().release());
}

}  // namespace WTF
