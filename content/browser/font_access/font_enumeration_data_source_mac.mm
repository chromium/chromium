// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_enumeration_data_source_mac.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>

#include <string>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"

namespace content {

namespace {

base::apple::ScopedCFTypeRef<CFStringRef> GetLocalizedString(
    CTFontDescriptorRef fd,
    CFStringRef attribute) {
  return base::apple::ScopedCFTypeRef<CFStringRef>(
      base::apple::CFCast<CFStringRef>(CTFontDescriptorCopyLocalizedAttribute(
          fd, attribute, /*language=*/nullptr)));
}

base::apple::ScopedCFTypeRef<CFStringRef> GetString(CTFontDescriptorRef fd,
                                                    CFStringRef attribute) {
  return base::apple::ScopedCFTypeRef<CFStringRef>(
      base::apple::CFCast<CFStringRef>(
          CTFontDescriptorCopyAttribute(fd, attribute)));
}

}  // namespace

FontEnumerationDataSourceMac::FontEnumerationDataSourceMac() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FontEnumerationDataSourceMac::~FontEnumerationDataSourceMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FontEnumerationDataSourceMac::IsValidFontMac(
    const CTFontDescriptorRef& fd) {
  base::apple::ScopedCFTypeRef<CFStringRef> cf_postscript_name =
      GetString(fd, kCTFontNameAttribute);
  base::apple::ScopedCFTypeRef<CFStringRef> cf_full_name =
      GetLocalizedString(fd, kCTFontDisplayNameAttribute);
  base::apple::ScopedCFTypeRef<CFStringRef> cf_family =
      GetString(fd, kCTFontFamilyNameAttribute);
  base::apple::ScopedCFTypeRef<CFStringRef> cf_style =
      GetString(fd, kCTFontStyleNameAttribute);

  if (!cf_postscript_name || !cf_full_name || !cf_family || !cf_style) {
    // Check for invalid attribute returns as MacOS may allow
    // OS-level installation of fonts for some of these.
    return false;
  }
  this->cf_postscript_name_ = cf_postscript_name;
  this->cf_full_name_ = cf_full_name;
  this->cf_family_ = cf_family;
  this->cf_style_ = cf_style;
  return true;
}

blink::FontEnumerationTable FontEnumerationDataSourceMac::GetFonts(
    const std::string& locale) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  blink::FontEnumerationTable font_enumeration_table;

  @autoreleasepool {
    NSDictionary* options = @{
      base::apple::CFToNSPtrCast(kCTFontCollectionRemoveDuplicatesOption) : @YES
    };

    base::apple::ScopedCFTypeRef<CTFontCollectionRef> collection(
        CTFontCollectionCreateFromAvailableFonts(
            base::apple::NSToCFPtrCast(options)));

    base::apple::ScopedCFTypeRef<CFArrayRef> font_descs(
        CTFontCollectionCreateMatchingFontDescriptors(collection.get()));

    // Used to filter duplicates.
    std::set<std::string> fonts_seen;

    for (CFIndex i = 0; i < CFArrayGetCount(font_descs.get()); ++i) {
      CTFontDescriptorRef fd = base::apple::CFCast<CTFontDescriptorRef>(
          CFArrayGetValueAtIndex(font_descs.get(), i));
      if (!IsValidFontMac(fd)) {
        // Skip invalid fonts.
        continue;
      }

      std::string postscript_name =
          base::SysCFStringRefToUTF8(cf_postscript_name_.get());

      auto it_and_success = fonts_seen.emplace(postscript_name);
      if (!it_and_success.second) {
        // Skip duplicate.
        continue;
      }

      blink::FontEnumerationTable_FontData* data =
          font_enumeration_table.add_fonts();
      data->set_postscript_name(std::move(postscript_name));
      data->set_full_name(base::SysCFStringRefToUTF8(cf_full_name_.get()));
      data->set_family(base::SysCFStringRefToUTF8(cf_family_.get()));
      data->set_style(base::SysCFStringRefToUTF8(cf_style_.get()));
    }

    return font_enumeration_table;
  }
}

}  // namespace content
