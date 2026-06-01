// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
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
      NSArray<GeminiConsentRow*>* rows =
          IsGeminiUpdatedConsentEnabled()
              ? [self standardRowsForManaged:isManaged useStrict:useStrict]
              : @[
                  [self standardFirstRowForManaged:isManaged],
                  [self standardSecondRowForManaged:isManaged],
                ];
      NSAttributedString* footnote = [self footnoteForCountry:country
                                                    useStrict:useStrict];
      return [[GeminiConsentConfiguration alloc] initWithRows:rows
                                                     footnote:footnote
                                                       header:nil
                                                  collapsible:useStrict];
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

// Returns an array of link attribute dictionaries for the given actions.
+ (NSArray<NSDictionary*>*)linkAttributesForActions:(NSArray<NSString*>*)actions
                                              style:(UIFontTextStyle)style {
  NSMutableArray* attributes = [NSMutableArray arrayWithCapacity:actions.count];
  for (NSString* action in actions) {
    NSDictionary* linkAttributes = @{
      NSLinkAttributeName : action,
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color],
      NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
      NSFontAttributeName :
          PreferredFontForTextStyle(style, UIFontWeightSemibold)
    };
    [attributes addObject:linkAttributes];
  }
  return attributes;
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

// Helper to construct body text using embedded link delimiters.
+ (NSAttributedString*)attributedTextForBody:(NSString*)text
                                     actions:(NSArray<NSString*>*)actions {
  NSDictionary* attributes = [self defaultTextAttributes];
  NSArray<NSDictionary*>* linkAttributes =
      [self linkAttributesForActions:actions style:UIFontTextStyleBody];
  return AttributedStringFromStringWithLinks(text, attributes, linkAttributes);
}

// Helper to construct footnote text using embedded link delimiters.
+ (NSAttributedString*)attributedTextForFooter:(NSString*)text
                                       actions:(NSArray<NSString*>*)actions {
  NSDictionary* attributes = [self footnoteTextAttributes];
  NSArray<NSDictionary*>* linkAttributes =
      [self linkAttributesForActions:actions style:UIFontTextStyleFootnote];
  return AttributedStringFromStringWithLinks(text, attributes, linkAttributes);
}

#pragma mark - New Standard FRE

// Builds, formats and orders the FRE consent rows.
//
// Layout structure:
// - Managed: [Tabs Sharing] -> [Enterprise Governance]
// - Normal:  [Tabs Sharing] -> [Standard Governance]
// - Strict:  [Tabs Sharing] -> [Connected Services] -> [Strict Governance]
+ (NSArray<GeminiConsentRow*>*)standardRowsForManaged:(BOOL)isManaged
                                            useStrict:(BOOL)useStrict {
  GeminiConsentRow* shareTabsRow = [self shareTabRowWithStrict:useStrict];
  GeminiConsentRow* governanceRow = [self dataGovernanceRowForStrict:useStrict
                                                             managed:isManaged];
  // Managed account layout is prioritized over the `useStrict` parameter.
  if (isManaged) {
    return @[ shareTabsRow, governanceRow ];
  }

  return useStrict
             ? @[ shareTabsRow, [self connectedServicesRow], governanceRow ]
             : @[ shareTabsRow, governanceRow ];
}

#pragma mark Share tab

// Builds the share tab row, with a configurable initial collapsed state based
// on strict mode.
+ (GeminiConsentRow*)shareTabRowWithStrict:(BOOL)useStrict {
  UIImage* icon = CustomSymbolWithConfiguration(
      kPhoneSparkleSymbol, [self defaultSymbolConfiguration]);
  NSString* title = l10n_util::GetNSString(
      useStrict ? IDS_IOS_GEMINI_CONSENT_SHARE_TAB_TITLE_STRICT
                : IDS_IOS_GEMINI_CONSENT_SHARE_TAB_TITLE);
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_SHARE_TAB_BODY);
  NSAttributedString* body =
      [[NSAttributedString alloc] initWithString:text
                                      attributes:[self defaultTextAttributes]];
  GeminiConsentRow* row = [[GeminiConsentRow alloc] initWithIcon:icon
                                                           title:title
                                                            body:body];
  row.collapsed = !useStrict;
  return row;
}

#pragma mark Data governance

// Returns the appropriate Data Governance row based on strict, managed, and
// country parameters.
+ (GeminiConsentRow*)dataGovernanceRowForStrict:(BOOL)useStrict
                                        managed:(BOOL)isManaged {
  // Managed account layout is prioritized over the `useStrict` parameter.
  if (isManaged) {
    return [self dataGovernanceRowForManaged];
  }

  return useStrict ? [self dataGovernanceRowForStrict]
                   : [self dataGovernanceRowForNormal];
}

// Builds the enterprise row for managed accounts.
+ (GeminiConsentRow*)dataGovernanceRowForManaged {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kBuilding2Symbol, [self defaultSymbolConfiguration]);
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_MANAGED_TITLE);
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_MANAGED_BODY);
  NSAttributedString* body =
      [self attributedTextForBody:text
                          actions:@[ kGeminiDataGovernanceManagedLinkAction ]];
  GeminiConsentRow* row = [[GeminiConsentRow alloc] initWithIcon:icon
                                                           title:title
                                                            body:body];
  row.collapsed = NO;
  return row;
}

// Builds the data governance row for non managed layout.
+ (GeminiConsentRow*)dataGovernanceRowForNormal {
  UIImage* icon = DefaultSymbolWithConfiguration(
      [self secondSymbolNameForManaged:NO], [self defaultSymbolConfiguration]);
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_NON_MANAGED_TITLE);
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_NON_MANAGED_BODY);
  NSAttributedString* body =
      [self attributedTextForBody:text
                          actions:@[
                            kGeminiDataGovernanceNormalChoicesLinkAction,
                            kGeminiDataGovernanceNormalLocationLinkAction
                          ]];
  GeminiConsentRow* row = [[GeminiConsentRow alloc] initWithIcon:icon
                                                           title:title
                                                            body:body];
  row.collapsed = NO;
  return row;
}

// Builds the data governance row for non managed strict layout.
+ (GeminiConsentRow*)dataGovernanceRowForStrict {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kHistorySymbol, [self defaultSymbolConfiguration]);
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_DATA_GORVERNANCE_NON_MANAGED_TITLE);

  NSMutableAttributedString* body = [[NSMutableAttributedString alloc] init];

  NSDictionary* defaultAttrs = [self defaultTextAttributes];
  NSMutableDictionary* boldAttrs = [defaultAttrs mutableCopy];
  boldAttrs[NSFontAttributeName] =
      PreferredFontForTextStyle(UIFontTextStyleBody, UIFontWeightBold);

  // Subsection 1
  NSString* subtitle1 = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_SUBTITLE_1);
  [body appendAttributedString:[[NSAttributedString alloc]
                                   initWithString:subtitle1
                                       attributes:boldAttrs]];
  [[body mutableString] appendString:@"\n"];

  NSString* text1 = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_BODY_1);
  [body appendAttributedString:[[NSAttributedString alloc]
                                   initWithString:text1
                                       attributes:defaultAttrs]];
  [[body mutableString] appendString:@"\n\n"];

  // Subsection 2
  NSString* subtitle2 = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_SUBTITLE_2);
  [body appendAttributedString:[[NSAttributedString alloc]
                                   initWithString:subtitle2
                                       attributes:boldAttrs]];
  [[body mutableString] appendString:@"\n"];

  NSString* text2 = l10n_util::GetNSString(
      IDS_IOS_GEMINI_CONSENT_STRICT_DATA_GOVERNANCE_BODY_2);
  NSAttributedString* paragraph2 =
      [self attributedTextForBody:text2
                          actions:@[ kGeminiDataGovernanceStrictLinkAction ]];
  [body appendAttributedString:paragraph2];

  GeminiConsentRow* row = [[GeminiConsentRow alloc] initWithIcon:icon
                                                           title:title
                                                            body:body];
  row.collapsed = YES;
  return row;
}

#pragma mark Connected services

//  Builds the connected services row used for the strict layout.
+ (GeminiConsentRow*)connectedServicesRow {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kPuzzlePieceExtensionSymbol, [self defaultSymbolConfiguration]);
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_CONNECTED_SERVICES_TITLE);
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_CONSENT_CONNECTED_SERVICES_BODY);
  NSAttributedString* body =
      [self attributedTextForBody:text
                          actions:@[ kGeminiConnectedServicesLinkAction ]];
  GeminiConsentRow* row = [[GeminiConsentRow alloc] initWithIcon:icon
                                                           title:title
                                                            body:body];
  row.collapsed = YES;
  return row;
}

#pragma mark - Old Standard FRE

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
    NSString* text =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY);
    body = [self
        attributedTextForBody:text
                      actions:@[ kGeminiSecondBoxLinkActionManagedAccount ]];
  } else {
    NSString* text =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY);
    body = [self attributedTextForBody:text
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
      isKorea ? IDS_IOS_GEMINI_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA
              : IDS_IOS_GEMINI_CONSENT_FOOTNOTE_TEXT);

  NSArray<NSString*>* actions = isKorea ? @[
    kGeminiFirstFootnoteLinkAction,
    kGeminiKoreanTermsLinkAction,
    kGeminiSecondFootnoteLinkAction,
  ] : @[
    kGeminiFirstFootnoteLinkAction,
    kGeminiSecondFootnoteLinkAction,
  ];

  NSMutableAttributedString* footnote =
      [[self attributedTextForFooter:baseText actions:actions] mutableCopy];

  if ([country isEqualToString:kUSCountryCode]) {
    NSString* addition = l10n_util::GetNSString(
        IDS_IOS_GEMINI_CONSENT_FOOTNOTE_US_ONLY_ADDITION);
    [[footnote mutableString] appendString:@" "];
    [[footnote mutableString] appendString:addition];
  }

  if (useStrict) {
    NSMutableAttributedString* strictFootnote = [[self
        attributedTextForFooter:l10n_util::GetNSString(
                                    IDS_IOS_GEMINI_CONSENT_FOOTNOTE_WATCH_LABEL)
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
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_GEMINI_LIVE_CONSENT_TITLE)
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
  NSString* bodyText =
      l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_CONSENT_FIRST_BOX_BODY);
  NSAttributedString* body =
      [[NSAttributedString alloc] initWithString:bodyText
                                      attributes:[self defaultTextAttributes]];
  return [[GeminiConsentRow alloc] initWithIcon:icon title:nil body:body];
}

// Builds the second live FRE consent row.
+ (GeminiConsentRow*)liveSecondRow {
  UIImage* icon = DefaultSymbolWithConfiguration(
      kInfoCircleSymbol, [self defaultSymbolConfiguration]);
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_CONSENT_SECOND_BOX_BODY);
  NSAttributedString* body =
      [self attributedTextForBody:text
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
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_GEMINI_LIVE_CONSENT_THIRD_BOX_BODY);
  NSAttributedString* body =
      [self attributedTextForBody:text
                          actions:@[ kGeminiLivePrivacyPolicyLinkAction ]];
  return [[GeminiConsentRow alloc] initWithIcon:icon title:nil body:body];
}

@end
