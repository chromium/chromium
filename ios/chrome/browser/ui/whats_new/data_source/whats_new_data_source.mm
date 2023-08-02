// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"

#import "base/apple/bundle_locations.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/version.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// The size of the icon image.
const CGFloat kIconImageWhatsNew = 16;

// The file name.
NSString* const kfileName = @"whats_new_entries.plist";
NSString* const kfileNameM116 = @"whats_new_entries_m116.plist";

// Dictionary keys.
NSString* const kDictionaryFeaturesKey = @"Features";
NSString* const kDictionaryChromeTipsKey = @"ChromeTips";
NSString* const kDictionaryTypeKey = @"Type";
NSString* const kDictionaryTitleKey = @"Title";
NSString* const kDictionarySubtitleKey = @"Subtitle";
NSString* const kDictionaryIsSymbolKey = @"IsSymbol";
NSString* const kDictionaryIsSystemSymbolKey = @"IsSystemSymbol";
NSString* const kDictionaryBannerImageKey = @"BannerImageName";
NSString* const kDictionaryImageNameKey = @"ImageName";
NSString* const kDictionaryImageTextKey = @"ImageTexts";
NSString* const kDictionaryHeroBannerImageKey = @"HeroBannerImageName";
NSString* const kDictionaryIconImageKey = @"IconImageName";
NSString* const kDictionaryBackgroundColorKey = @"IconBackgroundColor";
NSString* const kDictionaryInstructionsKey = @"InstructionSteps";
NSString* const kDictionaryPrimaryActionKey = @"PrimaryActionTitle";
NSString* const kDictionaryLearnMoreURLKey = @"LearnMoreUrlString";

// Returns the UIColor corresponding to `color`.
UIColor* GenerateColor(NSString* color) {
  if ([color isEqualToString:@"blue"]) {
    return [UIColor colorNamed:kBlue500Color];
  } else if ([color isEqualToString:@"pink"]) {
    return [UIColor colorNamed:kPink400Color];
  } else if ([color isEqualToString:@"yellow"]) {
    return [UIColor colorNamed:kYellow500Color];
  } else if ([color isEqualToString:@"black"]) {
    return [UIColor colorNamed:kGrey800Color];
  } else if ([color isEqualToString:@"purple"]) {
    return [UIColor colorNamed:kPurple500Color];
  } else {
    return nil;
  }
}

// Returns a UIImage given an image name.
UIImage* GenerateImage(BOOL is_symbol, NSString* image, BOOL is_system_symbol) {
  if (is_symbol) {
    if (is_system_symbol) {
      return DefaultSymbolTemplateWithPointSize(image, kIconImageWhatsNew);
    }
    return CustomSymbolTemplateWithPointSize(image, kIconImageWhatsNew);
  }

  return [UIImage imageNamed:image];
}

// Returns a localized string array given `instructions`.
NSArray<NSString*>* GenerateLocalizedInstructions(NSArray* instructions) {
  NSMutableArray<NSString*>* localized_instructions =
      [[NSMutableArray alloc] init];
  for (NSObject* instruction in instructions) {
    NSNumber* instruction_id = base::mac::ObjCCast<NSNumber>(instruction);
    if (!instruction_id) {
      return nil;
    }

    [localized_instructions
        addObject:l10n_util::GetNSString([instruction_id intValue])];
  }
  return localized_instructions;
}

WhatsNewType WhatsNewTypeFromInt(int type) {
  const int min_value = static_cast<int>(WhatsNewType::kMinValue);
  const int max_value = static_cast<int>(WhatsNewType::kMaxValue);

  if (min_value > type || type > max_value) {
    NOTREACHED() << "unexpected type: " << type;
    return WhatsNewType::kError;
  }

  return static_cast<WhatsNewType>(type);
}

NSArray<WhatsNewItem*>* WhatsNewItemsFromFileAndKey(NSString* path,
                                                    NSString* key) {
  NSDictionary* entries = [NSDictionary dictionaryWithContentsOfFile:path];
  NSMutableArray<WhatsNewItem*>* items = [[NSMutableArray alloc] init];

  NSArray<NSObject*>* keys = entries[key];
  if (!keys) {
    return items;
  }

  for (NSObject* entry_key in keys) {
    NSDictionary* entry = base::mac::ObjCCast<NSDictionary>(entry_key);
    if (!entry) {
      continue;
    }

    WhatsNewItem* item = ConstructWhatsNewItem(entry);
    if (!item) {
      continue;
    }

    [items addObject:item];
  }
  return items;
}

NSArray<NSString*>* loadInstructionsForPasswordInOtherApps(
    NSArray<NSString*>* instructions) {
#if defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_16_0
  if (@available(iOS 16.0, *)) {
    return @[
      l10n_util::GetNSString(
          IDS_IOS_WHATS_NEW_CHROME_TIP_PASSWORDS_IN_OTHER_APPS_STEP_1_IOS16),
      l10n_util::GetNSString(
          IDS_IOS_WHATS_NEW_CHROME_TIP_PASSWORDS_IN_OTHER_APPS_STEP_2_IOS16),
      l10n_util::GetNSString(
          IDS_IOS_WHATS_NEW_CHROME_TIP_PASSWORDS_IN_OTHER_APPS_STEP_2),
    ];
  }
#endif  // defined (__IPHONE_16_0)
  return instructions;
}

}  // namespace

NSArray<WhatsNewItem*>* WhatsNewFeatureEntries(NSString* path) {
  return WhatsNewItemsFromFileAndKey(path, kDictionaryFeaturesKey);
}

NSArray<WhatsNewItem*>* WhatsNewChromeTipEntries(NSString* path) {
  return WhatsNewItemsFromFileAndKey(path, kDictionaryChromeTipsKey);
}

WhatsNewItem* ConstructWhatsNewItem(NSDictionary* entry) {
  // Load the entry type.
  NSNumber* type_value =
      base::mac::ObjCCast<NSNumber>(entry[kDictionaryTypeKey]);
  if (!type_value) {
    return nil;
  }
  WhatsNewType type = WhatsNewTypeFromInt([type_value intValue]);
  // Do not create a WhatsNewItem with an inalid type.
  if (type == WhatsNewType::kError) {
    return nil;
  }

  WhatsNewItem* whats_new_item = [[WhatsNewItem alloc] init];
  whats_new_item.type = type;

  // Load the entry title.
  NSNumber* title = base::mac::ObjCCast<NSNumber>(entry[kDictionaryTitleKey]);
  if (!title) {
    return nil;
  }
  whats_new_item.title = l10n_util::GetNSString([title intValue]);

  // Load the entry subtitle.
  NSNumber* subtitle =
      base::mac::ObjCCast<NSNumber>(entry[kDictionarySubtitleKey]);
  if (!subtitle) {
    return nil;
  }
  whats_new_item.subtitle = l10n_util::GetNSString([subtitle intValue]);

  // What's New M116 does not support hero banner or banner image.
  if (!IsWhatsNewM116Enabled()) {
    // Load the entry hero banner image.
    NSString* hero_banner_image = entry[kDictionaryHeroBannerImageKey];
    whats_new_item.heroBannerImage =
        [hero_banner_image length] == 0
            ? nil
            : GenerateImage(false, hero_banner_image, false);

    // Load the entry banner image.
    NSString* banner_image = entry[kDictionaryBannerImageKey];
    whats_new_item.bannerImage =
        [banner_image length] == 0 ? nil
                                   : GenerateImage(false, banner_image, false);
  }

  // Load the entry icon.
  BOOL is_symbol = [entry[kDictionaryIsSymbolKey] boolValue];
  BOOL is_system_symbol = [entry[kDictionaryIsSystemSymbolKey] boolValue];
  whats_new_item.iconImage = GenerateImage(
      is_symbol, entry[kDictionaryIconImageKey], is_system_symbol);

  // Load the entry icon background image.
  whats_new_item.backgroundColor =
      GenerateColor(entry[kDictionaryBackgroundColorKey]);

  // Load the entry instructions.
  NSArray<NSString*>* instructions =
      GenerateLocalizedInstructions(entry[kDictionaryInstructionsKey]);
  if (!instructions) {
    return nil;
  }

  // Special case for kPasswordsInOtherApps, which has a different first step
  // instruction on iOS 16.
  if (whats_new_item.type == WhatsNewType::kPasswordsInOtherApps) {
    whats_new_item.instructionSteps =
        loadInstructionsForPasswordInOtherApps(instructions);
  } else {
    whats_new_item.instructionSteps = instructions;
  }

  // Load the entry primary action title.
  NSNumber* primary_action_title =
      base::mac::ObjCCast<NSNumber>(entry[kDictionaryPrimaryActionKey]);
  if (!primary_action_title) {
    whats_new_item.primaryActionTitle = nil;
  } else {
    whats_new_item.primaryActionTitle =
        l10n_util::GetNSString([primary_action_title intValue]);
  }

  // Load the entry learn more url.
  NSString* url = entry[kDictionaryLearnMoreURLKey];
  if ([url length] > 0) {
    GURL gurl(base::SysNSStringToUTF8(url));
    [whats_new_item setLearnMoreURL:gurl];
  } else {
    [whats_new_item setLearnMoreURL:GURL::EmptyGURL()];
  }

  if (IsWhatsNewM116Enabled()) {
    // Load screenshot image.
    NSString* screenshot_image = entry[kDictionaryImageNameKey];
    whats_new_item.screenshotName = screenshot_image;

    // Load screenshot text provider.
    NSDictionary* screenshot_texts = entry[kDictionaryImageTextKey];
    NSMutableDictionary* screenshot_text_provider =
        [NSMutableDictionary dictionaryWithCapacity:screenshot_texts.count];
    for (id key in screenshot_texts) {
      NSNumber* val =
          base::mac::ObjCCast<NSNumber>([screenshot_texts objectForKey:key]);
      [screenshot_text_provider setValue:l10n_util::GetNSString([val intValue])
                                  forKey:key];
    }
    whats_new_item.screenshotTextProvider = screenshot_text_provider;
  }

  return whats_new_item;
}

NSString* WhatsNewFilePath() {
  NSString* bundle_path = [base::apple::FrameworkBundle() bundlePath];
  NSString* entries_file_path = [bundle_path
      stringByAppendingPathComponent:IsWhatsNewM116Enabled() ? kfileNameM116
                                                             : kfileName];
  return entries_file_path;
}
