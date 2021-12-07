// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/platform_mime_util.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

#if defined(OS_IOS)
#include <MobileCoreServices/MobileCoreServices.h>
#else
#include <CoreServices/CoreServices.h>
#endif  // defined(OS_IOS)

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext, std::string* result) const {
  std::string ext_nodot = ext;
  if (ext_nodot.length() >= 1 && ext_nodot[0] == L'.')
    ext_nodot.erase(ext_nodot.begin());
  base::ScopedCFTypeRef<CFStringRef> ext_ref(
      base::SysUTF8ToCFStringRef(ext_nodot));
  if (!ext_ref)
    return false;
  base::ScopedCFTypeRef<CFStringRef> uti(UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassFilenameExtension, ext_ref, nullptr));
  if (!uti)
    return false;
  base::ScopedCFTypeRef<CFStringRef> mime_ref(
      UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
  if (!mime_ref)
    return false;

  *result = base::SysCFStringRefToUTF8(mime_ref);
  return true;
}

bool PlatformMimeUtil::GetPlatformPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* ext) const {
  base::ScopedCFTypeRef<CFStringRef> mime_ref(
      base::SysUTF8ToCFStringRef(mime_type));
  if (!mime_ref)
    return false;
  base::ScopedCFTypeRef<CFStringRef> uti(UTTypeCreatePreferredIdentifierForTag(
      kUTTagClassMIMEType, mime_ref, nullptr));
  if (!uti)
    return false;
  base::ScopedCFTypeRef<CFStringRef> ext_ref(
      UTTypeCopyPreferredTagWithClass(uti, kUTTagClassFilenameExtension));
  if (!ext_ref)
    return false;

  *ext = base::SysCFStringRefToUTF8(ext_ref);
  return true;
}

void PlatformMimeUtil::GetPlatformExtensionsForMimeType(
    const std::string& mime_type,
    std::unordered_set<base::FilePath::StringType>* extensions) const {
  base::ScopedCFTypeRef<CFArrayRef> exts_ref;

  base::ScopedCFTypeRef<CFStringRef> mime_ref(
      base::SysUTF8ToCFStringRef(mime_type));
  if (mime_ref) {
    base::ScopedCFTypeRef<CFStringRef> uti(
        UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mime_ref,
                                              nullptr));
    if (uti) {
      exts_ref.reset(
          UTTypeCopyAllTagsWithClass(uti, kUTTagClassFilenameExtension));
    }
  }

  NSArray* extensions_list = base::mac::CFToNSCast(exts_ref);

  if (extensions_list) {
    for (NSString* extension in extensions_list)
      extensions->insert(base::SysNSStringToUTF8(extension));
  } else {
    // Huh? Give up.
    base::FilePath::StringType ext;
    if (GetPlatformPreferredExtensionForMimeType(mime_type, &ext))
      extensions->insert(ext);
  }
}

}  // namespace net
