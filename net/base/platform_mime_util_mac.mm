// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/platform_mime_util.h"

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
#include <MobileCoreServices/MobileCoreServices.h>
#else
#include <CoreServices/CoreServices.h>
#endif  // BUILDFLAG(IS_IOS)

namespace net {

bool PlatformMimeUtil::GetPlatformMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    std::string* result) const {
  std::string ext_nodot = ext;
  if (ext_nodot.length() >= 1 && ext_nodot[0] == L'.') {
    ext_nodot.erase(ext_nodot.begin());
  }

  if (@available(macOS 11, iOS 14, *)) {
    UTType* uttype =
        [UTType typeWithFilenameExtension:base::SysUTF8ToNSString(ext_nodot)];
    // Dynamic UTTypes are made by the system in the event that there's a
    // non-identifiable mime type. For now, we should treat dynamic UTTypes as a
    // nonstandard format.
    if ([uttype isDynamic] || uttype.preferredMIMEType == nil) {
      return false;
    }
    *result = base::SysNSStringToUTF8(uttype.preferredMIMEType);
    return true;
  } else {
    base::ScopedCFTypeRef<CFStringRef> ext_ref(
        base::SysUTF8ToCFStringRef(ext_nodot));
    if (!ext_ref) {
      return false;
    }
    base::ScopedCFTypeRef<CFStringRef> uti(
        UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                              ext_ref, nullptr));
    if (!uti) {
      return false;
    }
    base::ScopedCFTypeRef<CFStringRef> mime_ref(
        UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
    if (!mime_ref) {
      return false;
    }

    *result = base::SysCFStringRefToUTF8(mime_ref);
    return true;
  }
}

bool PlatformMimeUtil::GetPlatformPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* ext) const {
  if (@available(macOS 11, iOS 14, *)) {
    UTType* uttype =
        [UTType typeWithMIMEType:base::SysUTF8ToNSString(mime_type)];
    if ([uttype isDynamic] || uttype.preferredFilenameExtension == nil) {
      return false;
    }
    *ext = base::SysNSStringToUTF8(uttype.preferredFilenameExtension);
    return true;
  } else {
    base::ScopedCFTypeRef<CFStringRef> mime_ref(
        base::SysUTF8ToCFStringRef(mime_type));
    if (!mime_ref) {
      return false;
    }
    base::ScopedCFTypeRef<CFStringRef> uti(
        UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mime_ref,
                                              nullptr));
    if (!uti) {
      return false;
    }
    base::ScopedCFTypeRef<CFStringRef> ext_ref(
        UTTypeCopyPreferredTagWithClass(uti, kUTTagClassFilenameExtension));
    if (!ext_ref) {
      return false;
    }

    *ext = base::SysCFStringRefToUTF8(ext_ref);
    return true;
  }
}

void PlatformMimeUtil::GetPlatformExtensionsForMimeType(
    const std::string& mime_type,
    std::unordered_set<base::FilePath::StringType>* extensions) const {
  if (@available(macOS 11, iOS 14, *)) {
    NSArray<UTType*>* types =
        [UTType typesWithTag:base::SysUTF8ToNSString(mime_type)
                    tagClass:UTTagClassMIMEType
            conformingToType:nil];
    bool extensions_found = false;
    if (types) {
      NSInteger numberOfTypes = (NSInteger)types.count;
      for (NSInteger i = 0; i < numberOfTypes; ++i) {
        UTType* type = types[i];
        if (!type || type.preferredFilenameExtension == nil) {
          continue;
        }
        extensions_found = true;
        NSArray<NSString*>* extensions_list =
            type.tags[UTTagClassFilenameExtension];
        for (NSString* extension in extensions_list) {
          extensions->insert(base::SysNSStringToUTF8(extension));
        }
      }
    }

    if (extensions_found) {
      return;
    }

    base::FilePath::StringType ext;
    if (GetPlatformPreferredExtensionForMimeType(mime_type, &ext)) {
      extensions->insert(ext);
    }
  } else {
    base::ScopedCFTypeRef<CFStringRef> mime_ref(
        base::SysUTF8ToCFStringRef(mime_type));
    if (mime_ref) {
      bool extensions_found = false;
      base::ScopedCFTypeRef<CFArrayRef> types(UTTypeCreateAllIdentifiersForTag(
          kUTTagClassMIMEType, mime_ref, nullptr));
      if (types) {
        for (CFIndex i = 0; i < CFArrayGetCount(types); i++) {
          base::ScopedCFTypeRef<CFArrayRef> extensions_list(
              UTTypeCopyAllTagsWithClass(base::mac::CFCast<CFStringRef>(
                                             CFArrayGetValueAtIndex(types, i)),
                                         kUTTagClassFilenameExtension));
          if (!extensions_list) {
            continue;
          }
          extensions_found = true;
          for (NSString* extension in base::mac::CFToNSCast(extensions_list)) {
            extensions->insert(base::SysNSStringToUTF8(extension));
          }
        }
      }
      if (extensions_found) {
        return;
      }
    }

    // Huh? Give up.
    base::FilePath::StringType ext;
    if (GetPlatformPreferredExtensionForMimeType(mime_type, &ext)) {
      extensions->insert(ext);
    }
  }
}

}  // namespace net
