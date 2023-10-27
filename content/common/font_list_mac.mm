// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include <utility>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

// The code here is unusually skeptical about the macOS APIs returning non-null
// values. An earlier version was reverted due to crashing tests on bots running
// older macOS versions. The DCHECKs are there to expedite debugging similar
// problems.

namespace content {

namespace {

// Core Text-based localized family name lookup.
//
// This class is not thread-safe.
//
// This class caches some state for an efficient implementation of
// [NSFontManager localizedNameForFamily:face:] using the Core Text API.
class FontFamilyResolver {
 public:
  FontFamilyResolver() {
    DCHECK(mandatory_attributes_);
    DCHECK(font_descriptor_attributes_);
  }
  ~FontFamilyResolver() = default;

  FontFamilyResolver(const FontFamilyResolver&) = delete;
  FontFamilyResolver& operator=(const FontFamilyResolver&) = delete;

  // Returns a localized font family name for the given family name.
  base::apple::ScopedCFTypeRef<CFStringRef> CopyLocalizedFamilyName(
      CFStringRef family_name) {
    DCHECK(family_name);

    CFDictionarySetValue(font_descriptor_attributes_.get(),
                         kCTFontFamilyNameAttribute, family_name);
    base::apple::ScopedCFTypeRef<CTFontDescriptorRef> raw_descriptor(
        CTFontDescriptorCreateWithAttributes(
            font_descriptor_attributes_.get()));
    DCHECK(raw_descriptor)
        << "CTFontDescriptorCreateWithAttributes returned null";

    base::apple::ScopedCFTypeRef<CFArrayRef> normalized_descriptors(
        CTFontDescriptorCreateMatchingFontDescriptors(
            raw_descriptor.get(), mandatory_attributes_.get()));
    return CopyLocalizedFamilyNameFrom(family_name,
                                       normalized_descriptors.get());
  }

 private:
  // Returns the first font descriptor matching the given family name.
  //
  // `descriptors` must be an array of CTFontDescriptors whose font family name
  // attribute is populated.
  //
  // `descriptors` may be null, representing an empty array. This case is
  // handled because CTFontDescriptorCreateMatchingFontDescriptors() may
  // return null, even on macOS 11. Discovery documented in crbug.com/1235042.
  //
  // Returns null if none of the descriptors match.
  static base::apple::ScopedCFTypeRef<CTFontDescriptorRef>
  FindFirstWithFamilyName(CFStringRef family_name, CFArrayRef descriptors) {
    DCHECK(family_name != nullptr);

    CFIndex descriptor_count = descriptors ? CFArrayGetCount(descriptors) : 0;
    for (CFIndex i = 0; i < descriptor_count; ++i) {
      CTFontDescriptorRef descriptor =
          base::apple::CFCastStrict<CTFontDescriptorRef>(
              CFArrayGetValueAtIndex(descriptors, i));
      DCHECK(descriptor != nullptr)
          << "The descriptors array has a null element.";

      base::apple::ScopedCFTypeRef<CFStringRef> descriptor_family_name(
          base::apple::CFCastStrict<CFStringRef>(CTFontDescriptorCopyAttribute(
              descriptor, kCTFontFamilyNameAttribute)));
      if (CFStringCompare(family_name, descriptor_family_name.get(),
                          /*compareOptions=*/0) == kCFCompareEqualTo) {
        return base::apple::ScopedCFTypeRef<CTFontDescriptorRef>(
            descriptor, base::scoped_policy::RETAIN);
      }
    }
    return base::apple::ScopedCFTypeRef<CTFontDescriptorRef>(nullptr);
  }

  // Returns a localized font family name for the given family name.
  //
  // `descriptors` must be an array of normalized CTFontDescriptors representing
  // all the font descriptors on the system matching the given family name.
  //
  // `descriptors` may be null, representing an empty array. This case is
  // handled because CTFontDescriptorCreateMatchingFontDescriptors() may
  // return null, even on macOS 11. Discovery documented in crbug.com/1235042.
  //
  // The given family name is returned as a fallback, if none of the descriptors
  // match the desired font family name.
  static base::apple::ScopedCFTypeRef<CFStringRef> CopyLocalizedFamilyNameFrom(
      CFStringRef family_name,
      CFArrayRef descriptors) {
    DCHECK(family_name != nullptr);

    base::apple::ScopedCFTypeRef<CTFontDescriptorRef> descriptor =
        FindFirstWithFamilyName(family_name, descriptors);
    if (!descriptor) {
      DLOG(WARNING) << "Will use non-localized family name for font family: "
                    << family_name;
      return base::apple::ScopedCFTypeRef<CFStringRef>(
          family_name, base::scoped_policy::RETAIN);
    }

    base::apple::ScopedCFTypeRef<CFStringRef> localized_family_name(
        base::apple::CFCastStrict<CFStringRef>(
            CTFontDescriptorCopyLocalizedAttribute(descriptor.get(),
                                                   kCTFontFamilyNameAttribute,
                                                   /*language=*/nullptr)));
    // CTFontDescriptorCopyLocalizedAttribute() is only supposed to return null
    // if the desired attribute (family name) is not present.
    //
    // We found that the function may return null, even when given a normalized
    // font descriptor whose attribute (family name) exists --
    // FindFirstWithFamilyName() only returns descriptors whose non-localized
    // family name attribute is equal to a given string. Discovery documented in
    // crbug.com/1235090.
    if (!localized_family_name) {
      DLOG(WARNING) << "Will use non-localized family name for font family: "
                    << family_name;
      return base::apple::ScopedCFTypeRef<CFStringRef>(
          family_name, base::scoped_policy::RETAIN);
    }
    return localized_family_name;
  }

  // Creates the set stored in |mandatory_attributes_|.
  static base::apple::ScopedCFTypeRef<CFSetRef> CreateMandatoryAttributes() {
    CFStringRef set_values[] = {kCTFontFamilyNameAttribute};
    return base::apple::ScopedCFTypeRef<CFSetRef>(CFSetCreate(
        kCFAllocatorDefault, reinterpret_cast<const void**>(set_values),
        std::size(set_values), &kCFTypeSetCallBacks));
  }

  // Creates the mutable dictionary stored in |font_descriptor_attributes_|.
  static base::apple::ScopedCFTypeRef<CFMutableDictionaryRef>
  CreateFontDescriptorAttributes() {
    return base::apple::ScopedCFTypeRef<CFMutableDictionaryRef>(
        CFDictionaryCreateMutable(kCFAllocatorDefault, /*capacity=*/1,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
  }

  // Used for all CTFontDescriptorCreateMatchingFontDescriptors() calls.
  //
  // Caching this dictionary saves one dictionary creation per lookup.
  const base::apple::ScopedCFTypeRef<CFSetRef> mandatory_attributes_ =
      CreateMandatoryAttributes();

  // Used for all CTFontDescriptorCreateMatchingFontDescriptors() calls.
  //
  // This dictionary has exactly one key, kCTFontFamilyNameAttribute. The value
  // associated with the key is overwritten as needed.
  //
  // Caching this dictionary saves one dictionary creation per lookup.
  const base::apple::ScopedCFTypeRef<CFMutableDictionaryRef>
      font_descriptor_attributes_ = CreateFontDescriptorAttributes();
};

}  // namespace

base::Value::List GetFontList_SlowBlocking() {
  @autoreleasepool {
    FontFamilyResolver resolver;

    NSArray* family_names = base::apple::CFToNSOwnershipCast(
        CTFontManagerCopyAvailableFontFamilyNames());
    DCHECK(family_names != nil)
        << "CTFontManagerCopyAvailableFontFamilyNames returned null";

    // Maps localized font family names to non-localized names.
    NSMutableDictionary* family_name_map =
        [NSMutableDictionary dictionaryWithCapacity:family_names.count];
    for (NSString* family_name in family_names) {
      DCHECK(family_name != nil)
          << "CTFontManagerCopyAvailableFontFamilyNames returned an array with "
          << "a null element";

      base::apple::ScopedCFTypeRef<CFStringRef> cf_normalized_family_name =
          resolver.CopyLocalizedFamilyName(
              base::apple::NSToCFPtrCast(family_name));
      DCHECK(cf_normalized_family_name)
          << "FontFamilyResolver::CopyLocalizedFamilyName returned null";
      family_name_map[base::apple::CFToNSPtrCast(
          cf_normalized_family_name.get())] = family_name;
    }

    // The Apple documentation for CTFontManagerCopyAvailableFontFamilyNames
    // states that it returns family names sorted for user interface display.
    // https://developer.apple.com/documentation/coretext/1499494-ctfontmanagercopyavailablefontfa
    //
    // This doesn't seem to be the case, at least on macOS 10.15.3.
    NSArray* sorted_localized_family_names = [family_name_map
        keysSortedByValueUsingSelector:@selector(localizedStandardCompare:)];

    base::Value::List font_list;
    for (NSString* localized_family_name in sorted_localized_family_names) {
      NSString* family_name = family_name_map[localized_family_name];

      base::Value::List font_list_item;
      font_list_item.reserve(2);
      font_list_item.Append(base::SysNSStringToUTF8(family_name));
      font_list_item.Append(base::SysNSStringToUTF8(localized_family_name));
      font_list.Append(std::move(font_list_item));
    }

    return font_list;
  }
}

}  // namespace content
