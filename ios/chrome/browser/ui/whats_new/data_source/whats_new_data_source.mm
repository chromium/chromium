// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/version.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {

// The size of the icon image.
const CGFloat kIconImageWhatsNew = 16;

// The file name.
NSString* const kfileName = @"whats_new_entries.plist";

// Dictionary keys.
NSString* const kDictionaryFeaturesKey = @"Features";
NSString* const kDictionaryChromeTipsKey = @"ChromeTips";
NSString* const kDictionaryTypeKey = @"Type";
NSString* const kDictionaryTitleKey = @"Title";
NSString* const kDictionarySubtitleKey = @"Subtitle";
NSString* const kDictionaryIsSymbolKey = @"IsSymbol";
NSString* const kDictionaryIsSystemSymbolKey = @"IsSystemSymbol";
NSString* const kDictionaryIsMulticolorSymbolKey = @"IsMulticolorSymbol";
NSString* const kDictionaryImageNameKey = @"ImageName";
NSString* const kDictionaryImageTextKey = @"ImageTexts";
NSString* const kDictionaryIconImageKey = @"IconImageName";
NSString* const kDictionaryBackgroundColorKey = @"IconBackgroundColor";
NSString* const kDictionaryInstructionsKey = @"InstructionSteps";
NSString* const kDictionaryPrimaryActionKey = @"PrimaryAction";
NSString* const kDictionaryLearnMoreURLKey = @"LearnMoreUrlString";
NSString* const kDictionaryIsIphoneOnlyKey = @"IsIphoneOnly";

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
  } else if ([color isEqualToString:@"green"]) {
    return [UIColor colorNamed:kGreen500Color];
  } else {
    return nil;
  }
}

// Returns the string for the primary button corresponding to the primary
// action.
NSString* GetPrimaryActionTitle(WhatsNewPrimaryAction action) {
  switch (action) {
    case WhatsNewPrimaryAction::kIOSSettings:
    case WhatsNewPrimaryAction::kIOSSettingsPasswords:
      return l10n_util::GetNSString(IDS_IOS_OPEN_IOS_SETTINGS);
    case WhatsNewPrimaryAction::kPrivacySettings:
    case WhatsNewPrimaryAction::kChromeSettings:
    case WhatsNewPrimaryAction::kSafeBrowsingSettings:
      return l10n_util::GetNSString(IDS_IOS_OPEN_CHROME_SETTINGS);
    case WhatsNewPrimaryAction::kChromePasswordManager:
      return l10n_util::GetNSString(IDS_IOS_OPEN_PASSWORD_MANAGER);
    case WhatsNewPrimaryAction::kLens:
      return l10n_util::GetNSString(IDS_IOS_GO_TO_LENS);
    case WhatsNewPrimaryAction::kNoAction:
    case WhatsNewPrimaryAction::kError:
      return nil;
  };
}

// Returns a UIImage given an image name.
UIImage* GenerateImage(BOOL is_symbol,
                       NSString* image,
                       BOOL is_system_symbol,
                       BOOL is_multicolor_symbol) {
  if (is_symbol) {
    if (is_system_symbol) {
      return DefaultSymbolTemplateWithPointSize(image, kIconImageWhatsNew);
    } else if (is_multicolor_symbol) {
      return MakeSymbolMulticolor(
          CustomSymbolWithPointSize(image, kIconImageWhatsNew));
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
    NSNumber* instruction_id = base::apple::ObjCCast<NSNumber>(instruction);
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
    NOTREACHED_IN_MIGRATION() << "unexpected type: " << type;
    return WhatsNewType::kError;
  }

  return static_cast<WhatsNewType>(type);
}

WhatsNewPrimaryAction WhatsNewPrimaryActionFromInt(int type) {
  const int min_value = static_cast<int>(WhatsNewPrimaryAction::kMinValue);
  const int max_value = static_cast<int>(WhatsNewPrimaryAction::kMaxValue);

  if (min_value > type || type > max_value) {
    NOTREACHED_IN_MIGRATION() << "unexpected type: " << type;
    return WhatsNewPrimaryAction::kError;
  }

  return static_cast<WhatsNewPrimaryAction>(type);
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
    NSDictionary* entry = base::apple::ObjCCast<NSDictionary>(entry_key);
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
  return @[
    l10n_util::GetNSString(
        IDS_IOS_WHATS_NEW_CHROME_TIP_PASSWORDS_IN_OTHER_APPS_STEP_1_IOS16),
    l10n_util::GetNSString(
        IDS_IOS_WHATS_NEW_CHROME_TIP_PASSWORDS_IN_OTHER_APPS_STEP_2_IOS16),
    l10n_util::GetNSString(
        IDS_IOS_WHATS_NEW_CHROME_TIP_PASSWORDS_IN_OTHER_APPS_STEP_2),
  ];
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
      base::apple::ObjCCast<NSNumber>(entry[kDictionaryTypeKey]);
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
  NSNumber* title = base::apple::ObjCCast<NSNumber>(entry[kDictionaryTitleKey]);
  if (!title) {
    return nil;
  }
  whats_new_item.title = l10n_util::GetNSString([title intValue]);

  // Load the entry subtitle.
  NSNumber* subtitle =
      base::apple::ObjCCast<NSNumber>(entry[kDictionarySubtitleKey]);
  if (!subtitle) {
    return nil;
  }
  whats_new_item.subtitle = l10n_util::GetNSString([subtitle intValue]);

  // Load the entry icon.
  BOOL is_symbol = [entry[kDictionaryIsSymbolKey] boolValue];
  BOOL is_system_symbol = [entry[kDictionaryIsSystemSymbolKey] boolValue];
  BOOL is_multicolor_symbol =
      [entry[kDictionaryIsMulticolorSymbolKey] boolValue];

  whats_new_item.iconImage =
      GenerateImage(is_symbol, entry[kDictionaryIconImageKey], is_system_symbol,
                    is_multicolor_symbol);

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

  // Load the entry primary action.
  NSNumber* primary_action =
      base::apple::ObjCCast<NSNumber>(entry[kDictionaryPrimaryActionKey]);
  if (!primary_action) {
    whats_new_item.primaryAction = WhatsNewPrimaryAction::kNoAction;
  } else {
    whats_new_item.primaryAction =
        WhatsNewPrimaryActionFromInt([primary_action intValue]);
  }

  if (whats_new_item.primaryAction == WhatsNewPrimaryAction::kError) {
    return nil;
  }

  // Load the entry primary action title.
  whats_new_item.primaryActionTitle =
      GetPrimaryActionTitle(whats_new_item.primaryAction);

  // Load the entry learn more url.
  NSString* url = entry[kDictionaryLearnMoreURLKey];
  if ([url length] > 0) {
    GURL gurl(base::SysNSStringToUTF8(url));
    [whats_new_item setLearnMoreURL:gurl];
  } else {
    [whats_new_item setLearnMoreURL:GURL()];
  }

  // Load screenshot image.
  NSString* screenshot_image = entry[kDictionaryImageNameKey];
  whats_new_item.screenshotName = screenshot_image;

  // Load screenshot text provider.
  NSDictionary* screenshot_texts = entry[kDictionaryImageTextKey];
  NSMutableDictionary* screenshot_text_provider =
      [NSMutableDictionary dictionaryWithCapacity:screenshot_texts.count];
  for (id key in screenshot_texts) {
    NSNumber* val =
        base::apple::ObjCCast<NSNumber>([screenshot_texts objectForKey:key]);
    [screenshot_text_provider setValue:l10n_util::GetNSString([val intValue])
                                forKey:key];
  }
  whats_new_item.screenshotTextProvider = screenshot_text_provider;

  // Load whether or not the feature is iPhone-only.
  BOOL is_iphone_only = [entry[kDictionaryIsIphoneOnlyKey] boolValue];
  whats_new_item.isIphoneOnly = is_iphone_only;

  return whats_new_item;
}

NSString* WhatsNewFilePath() {
  NSString* bundle_path = [base::apple::FrameworkBundle() bundlePath];
  NSString* entries_file_path =
      [bundle_path stringByAppendingPathComponent:kfileName];
  return entries_file_path;
}
