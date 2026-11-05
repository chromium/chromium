// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kWalletLogoHeight = 26.0;
constexpr CGFloat kWalletLogoSpacing = 6.0;

// Delimiters marking a tappable link in a string. Must match the tags parsed
// by `ParseStringWithLinks()` in ios/chrome/common/string_util.h.
NSString* const kBeginLinkTag = @"BEGIN_LINK";
NSString* const kEndLinkTag = @"END_LINK";

// Blank line separating the storage notice from the disclosure legal messages
// in the footer text.
NSString* const kStorageNoticeSeparator = @"\n\n";

// Line break separating consecutive disclosure legal messages in the footer
// text.
NSString* const kDisclosureLegalMessageSeparator = @"\n";

// Formats `legal_messages` with tappable link tags, appends their valid link
// URLs to `urls` (if non-nil) and their GURL values to `legal_message_urls`,
// and returns `storage_notice_text` combined with the formatted legal message
// lines separated by a blank line (or `storage_notice_text` unchanged when
// `legal_messages` has no non-empty lines).
NSString* CombineStorageNoticeWithLegalMessages(
    NSString* storage_notice_text,
    NSArray<AutofillLegalMessageLine*>* legal_messages,
    NSMutableArray<CrURL*>* urls,
    std::vector<GURL>& legal_message_urls) {
  if (legal_messages.count == 0) {
    return storage_notice_text;
  }

  NSMutableArray<CrURL*>* extracted_legal_urls = [NSMutableArray array];
  NSMutableArray<NSString*>* disclosure_legal_message_texts =
      [NSMutableArray arrayWithCapacity:legal_messages.count];
  for (AutofillLegalMessageLine* legal_message in legal_messages) {
    NSString* line_text =
        autofill::TextForDisclosureLegalMessageAppendingURLsTo(
            legal_message, extracted_legal_urls);
    if (line_text.length > 0) {
      [disclosure_legal_message_texts addObject:line_text];
    }
  }

  if (disclosure_legal_message_texts.count == 0) {
    return storage_notice_text;
  }

  [urls addObjectsFromArray:extracted_legal_urls];
  legal_message_urls.reserve(legal_message_urls.size() +
                             extracted_legal_urls.count);
  for (CrURL* url in extracted_legal_urls) {
    legal_message_urls.push_back(url.gurl);
  }

  NSString* legal_block = [disclosure_legal_message_texts
      componentsJoinedByString:kDisclosureLegalMessageSeparator];
  return [NSString stringWithFormat:@"%@%@%@", storage_notice_text,
                                    kStorageNoticeSeparator, legal_block];
}

}  // namespace

namespace autofill {

UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            bool is_personal_context,
                                            CGFloat symbol_point_size,
                                            UIColor* tint_color) {
  Symbol symbol;
  UIColor* color = tint_color ?: [UIColor colorNamed:kTextPrimaryColor];

  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      symbol = is_personal_context ? SymbolPassportSpark : SymbolPassport;
      break;
    case EntityTypeName::kDriversLicense:
    case EntityTypeName::kNationalIdCard:
      symbol = is_personal_context ? SymbolPersonTextRectangleSpark
                                   : SymbolPersonTextRectangle;
      break;
    case EntityTypeName::kVehicle:
      symbol = is_personal_context ? SymbolCarSpark : SymbolCar;
      break;
    case EntityTypeName::kKnownTravelerNumber:
    case EntityTypeName::kRedressNumber:
      symbol = is_personal_context ? SymbolPersonTextRectangle2Spark
                                   : SymbolPersonTextRectangle2;
      break;
    case EntityTypeName::kFlightReservation:
      symbol = is_personal_context ? SymbolAirplaneUpSpark : SymbolAirplaneUp;
      break;
    case EntityTypeName::kShipment:
      symbol = is_personal_context ? SymbolTruckBoxSpark : SymbolTruckBox;
      break;
    case EntityTypeName::kOrder:
      symbol = is_personal_context ? SymbolBagSpark : SymbolBag;
      break;
    default:
      return nil;
  }

  return SymbolWithPalette(SymbolWithPointSize(symbol, symbol_point_size),
                           @[ color ]);
}

NSString* DisplayNameForAutofillAiAttributeType(AttributeType attribute_type) {
  if (attribute_type.name() == AttributeTypeName::kVehicleVin) {
    return l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_VEHICLE_VIN_NAME);
  }
  return base::SysUTF16ToNSString(attribute_type.GetNameForI18n());
}

NSString* GetDialogTitleForSaveEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_PASSPORT_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_VEHICLE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_SAVE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetDialogTitleForUpdateEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_PASSPORT_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_DRIVERS_LICENSE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_NATIONAL_ID_CARD_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_VEHICLE_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_KNOWN_TRAVELER_NUMBER_ENTITY_DIALOG_TITLE);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(
          IDS_IOS_AUTOFILL_AI_UPDATE_REDRESS_NUMBER_ENTITY_DIALOG_TITLE);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetDialogTitleForAddEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_PASSPORT_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_ADD_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_VEHICLE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_ADD_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_ADD_REDRESS_NUMBER_ENTITY);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* GetDialogTitleForViewEntity(EntityTypeName entity_type_name) {
  return base::SysUTF16ToNSString(
      EntityType(entity_type_name).GetNameForI18n());
}

NSString* GetDialogTitleForEditEntity(EntityTypeName entity_type_name) {
  switch (entity_type_name) {
    case EntityTypeName::kPassport:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_PASSPORT_ENTITY);
    case EntityTypeName::kDriversLicense:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_EDIT_DRIVERS_LICENSE_ENTITY);
    case EntityTypeName::kNationalIdCard:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_EDIT_NATIONAL_ID_CARD_ENTITY);
    case EntityTypeName::kVehicle:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_VEHICLE_ENTITY);
    case EntityTypeName::kKnownTravelerNumber:
      return l10n_util::GetNSString(
          IDS_AUTOFILL_AI_EDIT_KNOWN_TRAVELER_NUMBER_ENTITY);
    case EntityTypeName::kRedressNumber:
      return l10n_util::GetNSString(IDS_AUTOFILL_AI_EDIT_REDRESS_NUMBER_ENTITY);
    default:
      return base::SysUTF16ToNSString(
          EntityType(entity_type_name).GetNameForI18n());
  }
}

NSString* WrapInLinkTags(NSString* text) {
  return
      [NSString stringWithFormat:@"%@%@%@", kBeginLinkTag, text, kEndLinkTag];
}

// TODO(crbug.com/573949431): Refactor to take `std::vector<GURL>&` instead of
// `NSMutableArray<CrURL*>*` once AutofillAIEntityEditTableViewController is
// refactored.
NSString* TextForDisclosureLegalMessageAppendingURLsTo(
    AutofillLegalMessageLine* legal_message,
    NSMutableArray<CrURL*>* urls) {
  NSString* lineText = legal_message.messageText;
  NSArray<NSValue*>* linkRanges = legal_message.linkRanges;
  const std::vector<GURL>& linkURLs = legal_message.linkURLs;

  NSMutableArray<NSString*>* fragments = [NSMutableArray array];
  NSUInteger currentIndex = 0;
  for (NSUInteger i = 0; i < linkRanges.count && i < linkURLs.size(); ++i) {
    NSRange range = [linkRanges[i] rangeValue];
    if (range.location == NSNotFound || range.location < currentIndex ||
        NSMaxRange(range) > lineText.length) {
      continue;
    }

    [fragments
        addObject:[lineText substringWithRange:NSMakeRange(currentIndex,
                                                           range.location -
                                                               currentIndex)]];

    NSString* linkText = [lineText substringWithRange:range];
    const GURL& url = linkURLs[i];
    if (url.is_valid()) {
      [fragments addObject:WrapInLinkTags(linkText)];
      [urls addObject:[[CrURL alloc] initWithGURL:url]];
    } else {
      [fragments addObject:linkText];
    }

    currentIndex = NSMaxRange(range);
  }

  [fragments addObject:[lineText substringFromIndex:currentIndex]];
  return [fragments componentsJoinedByString:@""];
}

AutofillAiEntityFooterContent GetEntityFooterContent(
    const EntityInstance& entity,
    bool is_update,
    const std::u16string& user_email,
    NSArray<AutofillLegalMessageLine*>* legal_messages) {
  NSMutableArray<CrURL*>* urls = [NSMutableArray array];
  NSString* storage_notice_text = nil;

  if (entity.record_type() != EntityInstance::RecordType::kServerWallet ||
      user_email.empty()) {
    storage_notice_text =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FOOTER_SAVE_TO_DEVICE);
  } else if (is_update) {
    storage_notice_text =
        GetUpdateEntitySavedInWalletFooterTextAppendingURLsTo(user_email, urls);
  } else {
    bool is_private_pass =
        GetWalletPassType(entity.type(), entity.record_type()) ==
        EntityInstance::WalletPassType::kPrivate;
    storage_notice_text = GetSaveEntityToWalletFooterTextAppendingURLsTo(
        user_email, is_private_pass, urls);
  }

  std::vector<GURL> legal_message_urls;
  NSString* footer_text = CombineStorageNoticeWithLegalMessages(
      storage_notice_text, legal_messages, urls, legal_message_urls);

  return AutofillAiEntityFooterContent{
      .text = footer_text,
      .urls = urls,
      .legal_message_urls = std::move(legal_message_urls),
  };
}

NSString* GetSaveEntityToWalletFooterTextAppendingURLsTo(
    const std::u16string& user_email,
    bool is_private_pass,
    NSMutableArray<CrURL*>* urls) {
  int stringId = GetSaveToWalletSubtitleStringId(is_private_pass);
  std::u16string googleWallet =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);

  if (stringId ==
      IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_PRIVATE_PASS) {
    NSString* howInfoIsUsedLink = WrapInLinkTags(l10n_util::GetNSString(
        IDS_AUTOFILL_HOW_THIS_INFO_IS_USED_ACROSS_GOOGLE_LINK));
    [urls addObject:[[CrURL alloc] initWithGURL:GetManageYourInfoURL()]];

    NSString* privacyPolicyLink = WrapInLinkTags(
        l10n_util::GetNSString(IDS_AUTOFILL_PRIVACY_POLICY_LINK));
    [urls addObject:[[CrURL alloc] initWithGURL:GetGooglePrivacyPolicyURL()]];

    return l10n_util::GetNSStringF(
        stringId, googleWallet, base::SysNSStringToUTF16(howInfoIsUsedLink),
        user_email, base::SysNSStringToUTF16(privacyPolicyLink));
  }

  NSString* formattedLink = WrapInLinkTags(
      l10n_util::GetNSString(IDS_AUTOFILL_MANAGE_YOUR_INFO_LINK));
  [urls addObject:[[CrURL alloc] initWithGURL:GetManageYourInfoURL()]];

  return l10n_util::GetNSStringF(stringId, googleWallet,
                                 base::SysNSStringToUTF16(formattedLink),
                                 googleWallet, user_email);
}

// TODO(crbug.com/573949431): Remove this overload once
// AutofillAIEntityEditTableViewController is refactored.
NSString* GetSaveEntityToWalletFooterTextAppendingURLsTo(
    NSString* user_email,
    bool is_private_pass,
    NSMutableArray<CrURL*>* urls) {
  return GetSaveEntityToWalletFooterTextAppendingURLsTo(
      base::SysNSStringToUTF16(user_email), is_private_pass, urls);
}

NSString* GetUpdateEntitySavedInWalletFooterTextAppendingURLsTo(
    const std::u16string& user_email,
    NSMutableArray<CrURL*>* urls) {
  NSString* google_wallet =
      l10n_util::GetNSString(IDS_AUTOFILL_GOOGLE_WALLET_TITLE);
  NSString* formatted_link = WrapInLinkTags(google_wallet);
  [urls addObject:[[CrURL alloc] initWithGURL:GetGoogleWalletPassesURL()]];
  return l10n_util::GetNSStringF(
      IDS_AUTOFILL_AI_UPDATE_ENTITY_TO_WALLET_DIALOG_SUBTITLE,
      base::SysNSStringToUTF16(formatted_link), user_email);
}

// TODO(crbug.com/573949431): Remove this overload once
// AutofillAIEntityEditTableViewController is refactored.
NSString* GetUpdateEntitySavedInWalletFooterTextAppendingURLsTo(
    NSString* user_email,
    NSMutableArray<CrURL*>* urls) {
  return GetUpdateEntitySavedInWalletFooterTextAppendingURLsTo(
      base::SysNSStringToUTF16(user_email), urls);
}

GURL GetManageYourInfoURL() {
  return GURL("https://support.google.com/wallet?p=private_use_across_google");
}

GURL GetGooglePrivacyPolicyURL() {
  return GURL("https://policies.google.com/privacy");
}

GURL GetGoogleWalletPassesURL() {
  return GURL("https://wallet.google.com/wallet/passes");
}

UIImage* GetWalletLogo(CGFloat point_size, UIColor* tint_color) {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  Symbol symbol =
      base::FeatureList::IsEnabled(features::kAutofillEnableGradientGoogleLogos)
          ? SymbolGoogleWalletIconV2
          : SymbolGoogleWalletIcon;
  return MakeSymbolMulticolor(SymbolWithPointSize(symbol, point_size));
#else
  return SymbolWithPalette(
      SymbolWithPointSize(SymbolSparkles, point_size),
      @[ tint_color ?: [UIColor colorNamed:kBlue600Color] ]);
#endif
}

UIView* CreateBrandedTitleForWalletSave(NSString* title) {
  BrandedNavigationItemTitleView* titleView =
      [[BrandedNavigationItemTitleView alloc] init];
  titleView.imageLogo = GetWalletLogo(kWalletLogoHeight, nil);
  titleView.title = title;
  titleView.titleLogoSpacing = kWalletLogoSpacing;
  titleView.accessibilityLabel = title;
  return titleView;
}

int GetSaveToWalletSubtitleStringId(bool is_private_pass) {
  if (is_private_pass &&
      base::FeatureList::IsEnabled(
          features::kAutofillRegionalizedPrivatePassesUxRefine)) {
    return IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_PRIVATE_PASS;
  }
  return IDS_AUTOFILL_AI_SAVE_ENTITY_TO_WALLET_DIALOG_SUBTITLE_NEW;
}

int GetSaveEntityAcceptButtonStringId() {
  return IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL;
}

}  // namespace autofill
