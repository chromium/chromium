// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreFoundation/CoreFoundation.h>

#include <ostream>
#include <string>
#include <string_view>

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "net/base/net_string_util.h"

namespace net {

namespace {

bool CharsetToCFStringEncoding(const char* charset,
                               CFStringEncoding* encoding) {
  if (charset == kCharsetLatin1 || strcmp(charset, kCharsetLatin1) == 0) {
    *encoding = kCFStringEncodingISOLatin1;
    return true;
  }
  // TODO(mattm): handle other charsets? See
  // https://developer.apple.com/reference/corefoundation/cfstringbuiltinencodings?language=objc
  // for list of standard CFStringEncodings.

  return false;
}

}  // namespace

// This constant cannot be defined as const char[] because it is initialized
// by base::kCodepageLatin1 (which is const char[]) in net_string_util_icu.cc.
const char* const kCharsetLatin1 = "ISO-8859-1";

bool ConvertToUtf8(std::string_view text,
                   const char* charset,
                   std::string* output) {
  CFStringEncoding encoding;
  if (!CharsetToCFStringEncoding(charset, &encoding))
    return false;

  base::apple::ScopedCFTypeRef<CFStringRef> cfstring(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(text.data()),
      base::checked_cast<CFIndex>(text.length()), encoding,
      /*isExternalRepresentation=*/false));
  if (!cfstring) {
    return false;
  }
  *output = base::SysCFStringRefToUTF8(cfstring.get());
  return true;
}

bool ConvertToUtf8AndNormalize(std::string_view text,
                               const char* charset,
                               std::string* output) {
  DCHECK(false) << "Not implemented yet.";
  return false;
}

bool ConvertToUTF16(std::string_view text,
                    const char* charset,
                    std::u16string* output) {
  DCHECK(false) << "Not implemented yet.";
  return false;
}

bool ConvertToUTF16WithSubstitutions(std::string_view text,
                                     const char* charset,
                                     std::u16string* output) {
  DCHECK(false) << "Not implemented yet.";
  return false;
}

bool ToUpper(std::u16string_view str, std::u16string* output) {
  base::apple::ScopedCFTypeRef<CFStringRef> cfstring =
      base::SysUTF16ToCFStringRef(str);
  base::apple::ScopedCFTypeRef<CFMutableStringRef> mutable_cfstring(
      CFStringCreateMutableCopy(kCFAllocatorDefault, /*maxLength=*/0,
                                cfstring.get()));
  CFStringUppercase(mutable_cfstring.get(), /*locale=*/nullptr);
  *output = base::SysCFStringRefToUTF16(mutable_cfstring.get());
  return true;
}

}  // namespace net
