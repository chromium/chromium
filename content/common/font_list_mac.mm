// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
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
    DCHECK(mandatory_attributes_ != nullptr);
    DCHECK(font_descriptor_attributes_ != nullptr);
  }
  ~FontFamilyResolver() = default;

  FontFamilyResolver(const FontFamilyResolver&) = delete;
  FontFamilyResolver& operator=(const FontFamilyResolver&) = delete;

  // Returns a localized font family name for the given family name.
  base::ScopedCFTypeRef<CFStringRef> CopyLocalizedFamilyName(
      CFStringRef family_name) {
    DCHECK(family_name != nullptr);

    CFDictionarySetValue(font_descriptor_attributes_.get(),
                         kCTFontFamilyNameAttribute, family_name);
    base::ScopedCFTypeRef<CTFontDescriptorRef> raw_descriptor(
        CTFontDescriptorCreateWithAttributes(
            font_descriptor_attributes_.get()));
    DCHECK(raw_descriptor != nullptr)
        << "CTFontDescriptorCreateWithAttributes returned null";

    base::ScopedCFTypeRef<CFArrayRef> normalized_descriptors(
        CTFontDescriptorCreateMatchingFontDescriptors(
            raw_descriptor, mandatory_attributes_.get()));
    return CopyLocalizedFamilyNameFrom(family_name,
                                       normalized_descriptors.get());
  }

  // True if the font should be hidden from Chrome.
  //
  // On macOS 10.15, CTFontManagerCopyAvailableFontFamilyNames() filters hidden
  // fonts. This is not true on older version of macOS that Chrome still
  // supports. The unittest FontTest.GetFontListDoesNotIncludeHiddenFonts can be
  // used to determine when it's safe to slim down / remove this function.
  static bool IsHiddenFontFamily(CFStringRef family_name) {
    DCHECK(family_name != nullptr);
    DCHECK_GT(CFStringGetLength(family_name), 0);

    // macOS 10.13 includes names that start with . (period). These fonts should
    // not be shown to users.
    return CFStringGetCharacterAtIndex(family_name, 0) == '.';
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
  static base::ScopedCFTypeRef<CTFontDescriptorRef> FindFirstWithFamilyName(
      CFStringRef family_name,
      CFArrayRef descriptors) {
    DCHECK(family_name != nullptr);

    CFIndex descriptor_count = descriptors ? CFArrayGetCount(descriptors) : 0;
    for (CFIndex i = 0; i < descriptor_count; ++i) {
      CTFontDescriptorRef descriptor =
          base::mac::CFCastStrict<CTFontDescriptorRef>(
              CFArrayGetValueAtIndex(descriptors, i));
      DCHECK(descriptor != nullptr)
          << "The descriptors array has a null element.";

      base::ScopedCFTypeRef<CFStringRef> descriptor_family_name(
          base::mac::CFCastStrict<CFStringRef>(CTFontDescriptorCopyAttribute(
              descriptor, kCTFontFamilyNameAttribute)));
      if (CFStringCompare(family_name, descriptor_family_name,
                          /*compareOptions=*/0) == kCFCompareEqualTo) {
        return base::ScopedCFTypeRef<CTFontDescriptorRef>(
            descriptor, base::scoped_policy::RETAIN);
      }
    }
    return base::ScopedCFTypeRef<CTFontDescriptorRef>(nullptr);
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
  static base::ScopedCFTypeRef<CFStringRef> CopyLocalizedFamilyNameFrom(
      CFStringRef family_name,
      CFArrayRef descriptors) {
    DCHECK(family_name != nullptr);

    base::ScopedCFTypeRef<CTFontDescriptorRef> descriptor =
        FindFirstWithFamilyName(family_name, descriptors);
    if (descriptor == nullptr) {
      DLOG(WARNING) << "Will use non-localized family name for font family: "
                    << family_name;
      return base::ScopedCFTypeRef<CFStringRef>(family_name,
                                                base::scoped_policy::RETAIN);
    }

    base::ScopedCFTypeRef<CFStringRef> localized_family_name(
        base::mac::CFCastStrict<CFStringRef>(
            CTFontDescriptorCopyLocalizedAttribute(descriptor,
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
    if (localized_family_name == nullptr) {
      DLOG(WARNING) << "Will use non-localized family name for font family: "
                    << family_name;
      return base::ScopedCFTypeRef<CFStringRef>(family_name,
                                                base::scoped_policy::RETAIN);
    }
    return localized_family_name;
  }

  // Creates the set stored in |mandatory_attributes_|.
  static base::ScopedCFTypeRef<CFSetRef> CreateMandatoryAttributes() {
    CFStringRef set_values[] = {kCTFontFamilyNameAttribute};
    return base::ScopedCFTypeRef<CFSetRef>(CFSetCreate(
        kCFAllocatorDefault, reinterpret_cast<const void**>(set_values),
        std::size(set_values), &kCFTypeSetCallBacks));
  }

  // Creates the mutable dictionary stored in |font_descriptor_attributes_|.
  static base::ScopedCFTypeRef<CFMutableDictionaryRef>
  CreateFontDescriptorAttributes() {
    return base::ScopedCFTypeRef<CFMutableDictionaryRef>(
        CFDictionaryCreateMutable(kCFAllocatorDefault, /*capacity=*/1,
                                  &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks));
  }

  // Used for all CTFontDescriptorCreateMatchingFontDescriptors() calls.
  //
  // Caching this dictionary saves one dictionary creation per lookup.
  const base::ScopedCFTypeRef<CFSetRef> mandatory_attributes_ =
      CreateMandatoryAttributes();

  // Used for all CTFontDescriptorCreateMatchingFontDescriptors() calls.
  //
  // This dictionary has exactly one key, kCTFontFamilyNameAttribute. The value
  // associated with the key is overwritten as needed.
  //
  // Caching this dictionary saves one dictionary creation per lookup.
  const base::ScopedCFTypeRef<CFMutableDictionaryRef>
      font_descriptor_attributes_ = CreateFontDescriptorAttributes();
};

}  // namespace

base::Value::List GetFontList_SlowBlocking() {
  @autoreleasepool {
    FontFamilyResolver resolver;

    base::ScopedCFTypeRef<CFArrayRef> cf_family_names(
        CTFontManagerCopyAvailableFontFamilyNames());
    DCHECK(cf_family_names != nullptr)
        << "CTFontManagerCopyAvailableFontFamilyNames returned null";
    NSArray* family_names = base::mac::CFToNSCast(cf_family_names.get());

    // Maps localized font family names to non-localized names.
    NSMutableDictionary* family_name_map = [NSMutableDictionary
        dictionaryWithCapacity:CFArrayGetCount(cf_family_names)];
    for (NSString* family_name in family_names) {
      DCHECK(family_name != nullptr)
          << "CTFontManagerCopyAvailableFontFamilyNames returned an array with "
          << "a null element";

      CFStringRef family_name_cf = base::mac::NSToCFCast(family_name);
      if (FontFamilyResolver::IsHiddenFontFamily(family_name_cf))
        continue;

      base::ScopedCFTypeRef<CFStringRef> cf_normalized_family_name =
          resolver.CopyLocalizedFamilyName(family_name_cf);
      DCHECK(cf_normalized_family_name != nullptr)
          << "FontFamilyResolver::CopyLocalizedFamilyName returned null";
      family_name_map[base::mac::CFToNSCast(cf_normalized_family_name)] =
          family_name;
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
