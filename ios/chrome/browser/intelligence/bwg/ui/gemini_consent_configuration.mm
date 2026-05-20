// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing and icon size attributes.
const CGFloat kIconSize = 16.0;
const CGFloat kLiveHeaderIconPointSize = 24.0;

// ISO alpha-2 country codes. Lowercased to match `base::ToLowerASCII`.
static NSString* const kSouthKoreaCountryCode = @"kr";
static NSString* const kUSCountryCode = @"us";

// Non-shared symbol names.
NSString* const kWarningShieldSymbol = @"exclamationmark.shield";

}  // namespace

@implementation GeminiConsentHeader

- (instancetype)initWithIcon:(UIImage*)icon title:(NSAttributedString*)title {
  self = [super init];
  if (self) {
    _icon = icon;
    _title = [title copy];
  }
  return self;
}

@end

@implementation GeminiConsentRow

- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                        body:(NSAttributedString*)body {
  self = [super init];
  if (self) {
    _icon = icon;
    _title = [title copy];
    _body = [body copy];
    _collapsed = YES;
  }
  return self;
}

@end

@implementation GeminiConsentConfiguration

- (instancetype)initWithRows:(NSArray<GeminiConsentRow*>*)rows
                    footnote:(NSAttributedString*)footnote
                      header:(GeminiConsentHeader*)header
                 collapsible:(BOOL)collapsible {
  self = [super init];
  if (self) {
    _rows = [rows copy];
    _footnote = [footnote copy];
    _header = header;
    _collapsible = collapsible;
  }
  return self;
}

+ (instancetype)configurationForManaged:(BOOL)isManaged
                                 strict:(BOOL)useStrict
                                   type:(GeminiFREType)type
                                country:(NSString*)country {
  switch (type) {
    case GeminiFREType::kLive: {
      NSArray<GeminiConsentRow*>* rows = @[
        [self liveFirstRow],
        [self liveSecondRow],
        [self liveThirdRow],
      ];
      return [[GeminiConsentConfiguration alloc] initWithRows:rows
                                                     footnote:nil
                                                       header:[self liveHeader]
                                                  collapsible:NO];
    }
    case GeminiFREType::kNewUser: {
      NSArray<GeminiConsentRow*>* rows = @[
        [self standardFirstRowForManaged:isManaged],
        [self standardSecondRowForManaged:isManaged],
      ];

      NSAttributedString* footnote = [self footnoteForCountry:country
                                                    useStrict:useStrict];

      return [[GeminiConsentConfiguration alloc] initWithRows:rows
                                                     footnote:footnote
                                                       header:nil
                                                  collapsible:NO];
    }
  }
}

#pragma mark - Private Helper Class Methods

// Returns the default UIImageSymbolConfiguration.
+ (UIImageSymbolConfiguration*)defaultSymbolConfiguration {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightMedium];
}

// Returns the default text attributes for consent body text.
+ (NSDictionary*)defaultTextAttributes {
  return @{
    NSFontAttributeName : PreferredFontForTextStyle(UIFontTextStyleBody),
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
}

// Returns the footnote text attributes.
+ (NSDictionary*)footnoteTextAttributes {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  return @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraphStyle,
  };
}

// Returns the text attributes for interactive links.
+ (NSDictionary*)linkAttributesForAction:(NSString*)action
                               fontStyle:(UIFontTextStyle)fontStyle {
  return @{
    NSLinkAttributeName : action,
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
    NSFontAttributeName :
        PreferredFontForTextStyle(fontStyle, UIFontWeightSemibold)
  };
}

// Gets the second SF Symbol name based on accounts type and iOS availability.
+ (NSString*)secondSymbolNameForManaged:(BOOL)isManaged {
  if (isManaged) {
    return kBuilding2Symbol;
  }
  if (@available(iOS 18, *)) {
    return kCounterClockWiseSymbol;
  }
  return kHistorySymbol;
}

// Creates an attributed string by resolving placeholders and formatting
// hyperlinks.
+ (NSAttributedString*)attributedTextWithText:(NSString*)text
                                        links:(NSArray<NSString*>*)links
                                      actions:(NSArray<NSString*>*)actions
                               textAttributes:(NSDictionary*)textAttributes
                                    fontStyle:(UIFontTextStyle)fontStyle {
  std::vector<std::u16string> substitutions;
  for (NSString* linkText in links) {
    substitutions.push_back(base::SysNSStringToUTF16(linkText));
  }

  std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
      base::SysNSStringToUTF16(text), substitutions, nullptr);
  NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:fullText
                                             attributes:textAttributes];

  // Looks for the first associated link string, assuming their uniqueness.
  auto styleLinks = ^(NSString* link, NSUInteger idx, BOOL* stop) {
    NSRange range = [fullText rangeOfString:link];
    if (range.location != NSNotFound) {
      NSDictionary* attrs = [self linkAttributesForAction:actions[idx]
                                                fontStyle:fontStyle];
      [attributedText addAttributes:attrs range:range];
    }
  };
  [links enumerateObjectsUsingBlock:styleLinks];

  return [attributedText copy];
}

// Helper to construct body text.
+ (NSAttributedString*)bodyAttributedTextForText:(NSString*)text
                                           links:(NSArray<NSString*>*)links
                                         actions:(NSArray<NSString*>*)actions {
  return [self attributedTextWithText:text
                                links:links
                              actions:actions
                       textAttributes:[self defaultTextAttributes]
                            fontStyle:UIFontTextStyleBody];
}

// Helper to construct footnote text.
+ (NSAttributedString*)footerAttributedTextForText:(NSString*)text
                                             links:(NSArray<NSString*>*)links
                                           actions:
                                               (NSArray<NSString*>*)actions {
  return [self attributedTextWithText:text
                                links:links
                              actions:actions
                       textAttributes:[self footnoteTextAttributes]
                            fontStyle:UIFontTextStyleFootnote];
}

#pragma mark - Standard FRE

// Builds the first standard FRE consent row.
+ (GeminiConsentRow*)standardFirstRowForManaged:(BOOL)isManaged {
  UIImage* icon = CustomSymbolWithConfiguration(
      kPhoneSparkleSymbol, [self defaultSymbolConfiguration]);
  NSString* title = l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FIRST_BOX_TITLE);
  NSString* bodyText = l10n_util::GetNSString(
      isManaged ? IDS_IOS_BWG_CONSENT_MANAGED_FIRST_BOX_BODY
                : IDS_IOS_BWG_CONSENT_NON_MANAGED_FIRST_BOX_BODY);
  NSAttributedString* body =
      [[NSAttributedString alloc] initWithString:bodyText
                                      attributes:[self defaultTextAttributes]];
  return [[GeminiConsentRow alloc] initWithIcon:icon title:title body:body];
}

// Builds the second standard FRE consent row.
+ (GeminiConsentRow*)standardSecondRowForManaged:(BOOL)isManaged {
  UIImage* icon = DefaultSymbolWithConfiguration(
      [self secondSymbolNameForManaged:isManaged],
      [self defaultSymbolConfiguration]);
  NSString* title = l10n_util::GetNSString(
      isManaged ? IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_TITLE
                : IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_TITLE);

  NSAttributedString* body;
  if (isManaged) {
    NSString* linkText = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY_LINK);
    NSString* text =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY);
    body = [self bodyAttributedTextForText:text
                                     links:@[ linkText ]
                                   actions:@[
                                     kGeminiSecondBoxLinkActionManagedAccount
                                   ]];
  } else {
    NSString* link1 = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY_LINK_1);
    NSString* link2 = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY_LINK_2);
    NSString* text =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY);
    body =
        [self bodyAttributedTextForText:text
                                  links:@[ link1, link2 ]
                                actions:@[
                                  kGeminiSecondBoxLink1ActionNonManagedAccount,
                                  kGeminiSecondBoxLink2ActionNonManagedAccount
                                ]];
  }

  return [[GeminiConsentRow alloc] initWithIcon:icon title:title body:body];
}

// Builds the footnote attributed string.
+ (NSAttributedString*)footnoteForCountry:(NSString*)country
                                useStrict:(BOOL)useStrict {
  BOOL isKorea =
      country &&
      [country caseInsensitiveCompare:kSouthKoreaCountryCode] == NSOrderedSame;
  NSString* baseText = l10n_util::GetNSString(
      isKorea ? IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA
              : IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT);

  NSArray<NSString*>* links = isKorea ? @[
    l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_LINK_1),
    l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_LINK_2),
    l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_LINK_3),
  ] : @[
    l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_LINK_1),
    l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_LINK_2),
  ];

  NSArray<NSString*>* actions = isKorea ? @[
    kGeminiFirstFootnoteLinkAction,
    kGeminiKoreanTermsLinkAction,
    kGeminiSecondFootnoteLinkAction,
  ] : @[
    kGeminiFirstFootnoteLinkAction,
    kGeminiSecondFootnoteLinkAction,
  ];

  NSMutableAttributedString* footnote =
      [[self footerAttributedTextForText:baseText links:links
                                 actions:actions] mutableCopy];

  if ([country isEqualToString:kUSCountryCode]) {
    NSString* addition =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_US_ONLY_ADDITION);
    [[footnote mutableString] appendString:@" "];
    [[footnote mutableString] appendString:addition];
  }

  if (useStrict) {
    NSString* watchLink =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_WATCH_LINK);
    NSMutableAttributedString* strictFootnote = [[self
        footerAttributedTextForText:
            l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_WATCH_LABEL)
                              links:@[ watchLink ]
                            actions:@[ kGeminiWatchLinkAction ]] mutableCopy];
    [[strictFootnote mutableString] appendString:@"\n\n"];
    [strictFootnote appendAttributedString:footnote];
    footnote = strictFootnote;
  }

  return [footnote copy];
}

#pragma mark - Live FRE

// Builds the custom header for the Live consent FRE.
+ (GeminiConsentHeader*)liveHeader {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kLiveHeaderIconPointSize
                          weight:UIImageSymbolWeightMedium];
  UIImage* icon;
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  icon = CustomSymbolWithConfiguration(kGeminiLiveLogoSymbol, config);
#else
  icon = DefaultSymbolWithConfiguration(kGeminiNonBrandedLogoSymbol, config);
#endif

  NSMutableAttributedString* attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:@"Go Live with Gemini"
              attributes:@{
                NSFontAttributeName : PreferredFontForTextStyle(
                    UIFontTextStyleTitle2, UIFontWeightRegular),
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kTextPrimaryColor]
              }];
  NSRange geminiRange = [attributedTitle.string rangeOfString:@"Gemini"];
  if (geminiRange.location != NSNotFound) {
    [attributedTitle addAttribute:NSForegroundColorAttributeName
                            value:[UIColor colorNamed:kBlue600Color]
                            range:geminiRange];
  }

  return [[GeminiConsentHeader alloc] initWithIcon:icon title:attributedTitle];
}

// Builds the first live FRE consent row.
+ (GeminiConsentRow*)liveFirstRow {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kMicrophoneSymbol, [self defaultSymbolConfiguration]);
  // TODO(crbug.com/498291812): Replace strings placeholders.
  NSString* bodyText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  NSAttributedString* body =
      [[NSAttributedString alloc] initWithString:bodyText
                                      attributes:[self defaultTextAttributes]];
  return [[GeminiConsentRow alloc] initWithIcon:icon title:nil body:body];
}

// Builds the second live FRE consent row.
+ (GeminiConsentRow*)liveSecondRow {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kInfoCircleSymbol, [self defaultSymbolConfiguration]);
  // TODO(crbug.com/498291812): Replace strings placeholders.
  NSString* fullText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua. "
      @"Gemini Apps Privacy Notice, Learn more.";

  NSAttributedString* body =
      [self bodyAttributedTextForText:fullText
                                links:@[
                                  @"Gemini Apps Privacy Notice",
                                  @"Learn more",
                                ]
                              actions:@[
                                kGeminiLivePrivacyNoticeLinkAction,
                                kGeminiLiveLearnMoreLinkAction
                              ]];
  return [[GeminiConsentRow alloc] initWithIcon:icon title:nil body:body];
}

// Builds the third live FRE consent row.
+ (GeminiConsentRow*)liveThirdRow {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kWarningShieldSymbol, [self defaultSymbolConfiguration]);
  // TODO(crbug.com/498291812): Replace strings placeholders.
  NSString* fullText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
      @"Respect other's privacy ipsum dolor sit amet.";

  NSAttributedString* body =
      [self bodyAttributedTextForText:fullText
                                links:@[ @"Respect other's privacy" ]
                              actions:@[ kGeminiLivePrivacyPolicyLinkAction ]];
  return [[GeminiConsentRow alloc] initWithIcon:icon title:nil body:body];
}

@end
