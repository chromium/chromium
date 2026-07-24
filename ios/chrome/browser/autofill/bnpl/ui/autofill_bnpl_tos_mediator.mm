// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_consumer.h"
#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_coordinator.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/resource/resource_bundle.h"

namespace {
// The size of the bullet icons in points.
constexpr CGFloat kBulletIconSize = 20.0;
// The Google Wallet domain URL string.
constexpr char16_t kWalletLinkText[] = u"wallet.google.com";
// The Google Wallet URL.
NSString* const kWalletUrlString = @"https://wallet.google.com/";
}  // namespace

@implementation AutofillBnplTosMediator {
  autofill::payments::BnplTosModel _model;
  std::unique_ptr<BnplCallbacks> _callbacks;
}

- (instancetype)initWithModel:(autofill::payments::BnplTosModel)model
                    callbacks:(std::unique_ptr<BnplCallbacks>)callbacks {
  self = [super init];
  if (self) {
    _model = std::move(model);
    _callbacks = std::move(callbacks);
  }
  return self;
}

- (void)setConsumer:(id<AutofillBnplTosConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self populateConsumer];
  }
}

- (void)didTapContinue {
  if (_callbacks && _callbacks->accept_callback) {
    std::move(_callbacks->accept_callback).Run();
  }
}

- (void)didTapCancel {
  if (_callbacks && _callbacks->cancel_callback) {
    std::move(_callbacks->cancel_callback).Run();
  }
}

#pragma mark - Private Helper Methods

- (void)populateConsumer {
  [_consumer setTitleText:[self titleText]];
  [_consumer setIssuerLogo:[self issuerLogo]];
  [_consumer setTermsBulletPoints:[self bulletPoints]];
  [_consumer setConsentText:[self consentText]];
}

// Formats the header title text based on linked/unlinked status.
- (NSString*)titleText {
  int titleResourceId = _model.issuer.payment_instrument().has_value()
                            ? IDS_AUTOFILL_BNPL_TOS_LINKED_TITLE
                            : IDS_AUTOFILL_BNPL_TOS_UNLINKED_TITLE;
  return l10n_util::GetNSStringF(titleResourceId,
                                 _model.issuer.GetDisplayName());
}

// Resolves and loads the issuer logo, supporting Light & Dark mode.
- (UIImage*)issuerLogo {
  auto iconPair = autofill::GetBnplIssuerIconIds(
      _model.issuer.issuer_id(),
      _model.issuer.payment_instrument().has_value());

  UIImage* lightImage = ui::ResourceBundle::GetSharedInstance()
                            .GetNativeImageNamed(iconPair.first.value())
                            .ToUIImage();
  UIImage* darkImage = ui::ResourceBundle::GetSharedInstance()
                           .GetNativeImageNamed(iconPair.second.value())
                           .ToUIImage();

  if (!lightImage) {
    return nil;
  }

  // Register light and dark mode versions dynamically into the image asset.
  UIImage* finalImage = [lightImage copy];
  if (darkImage) {
    [finalImage.imageAsset
              registerImage:darkImage
        withTraitCollection:[UITraitCollection
                                traitCollectionWithUserInterfaceStyle:
                                    UIUserInterfaceStyleDark]];
  }
  [finalImage.imageAsset
            registerImage:lightImage
      withTraitCollection:
          [UITraitCollection
              traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight]];

  return finalImage;
}

// Populates the three terms bullet point texts with symbols.
- (NSArray<AutofillBnplTosBulletItem*>*)bulletPoints {
  std::u16string displayName = _model.issuer.GetDisplayName();

  // Term 1 (Review)
  AutofillBnplTosBulletItem* item1 = [[AutofillBnplTosBulletItem alloc] init];
  NSString* term1String =
      l10n_util::GetNSStringF(IDS_AUTOFILL_BNPL_TOS_REVIEW_TEXT, displayName);
  item1.text = [[NSAttributedString alloc] initWithString:term1String];
  item1.icon =
      SymbolTemplateWithPointSize(SymbolPersonCropCircle, kBulletIconSize);

  // Term 2 (Eligibility check)
  AutofillBnplTosBulletItem* item2 = [[AutofillBnplTosBulletItem alloc] init];
  NSString* term2String =
      l10n_util::GetNSStringF(IDS_AUTOFILL_BNPL_TOS_APPROVE_TEXT, displayName);
  item2.text = [[NSAttributedString alloc] initWithString:term2String];
  item2.icon = SymbolTemplateWithPointSize(SymbolTextDocument, kBulletIconSize);

  // Term 3 (Linking benefits)
  AutofillBnplTosBulletItem* item3 = [[AutofillBnplTosBulletItem alloc] init];
  NSString* term3String = l10n_util::GetNSStringF(
      IDS_AUTOFILL_BNPL_TOS_LINK_TEXT, displayName, kWalletLinkText, nullptr);
  NSMutableAttributedString* term3Attributed =
      [[NSMutableAttributedString alloc] initWithString:term3String];
  NSRange linkRange =
      [term3String rangeOfString:base::SysUTF16ToNSString(kWalletLinkText)];
  if (linkRange.location != NSNotFound) {
    [term3Attributed addAttribute:NSLinkAttributeName
                            value:[NSURL URLWithString:kWalletUrlString]
                            range:linkRange];
  }
  item3.text = term3Attributed;
  item3.icon = SymbolTemplateWithPointSize(SymbolWalletBifold, kBulletIconSize);
  if (!item3.icon) {
    item3.icon = SymbolTemplateWithPointSize(SymbolCreditCard, kBulletIconSize);
  }

  return @[ item1, item2, item3 ];
}

// Converts LegalMessageLines to NSAttributedString.
- (NSAttributedString*)consentText {
  NSMutableAttributedString* result = [[NSMutableAttributedString alloc] init];

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  for (const auto& line : _model.legal_message_lines) {
    if (result.length > 0) {
      [result appendAttributedString:[[NSAttributedString alloc]
                                         initWithString:@"\n"]];
    }

    NSString* text = base::SysUTF16ToNSString(line.text());

    NSMutableAttributedString* lineAttributed =
        [[NSMutableAttributedString alloc] initWithString:text
                                               attributes:textAttributes];

    for (const auto& link : line.links()) {
      NSRange range = link.range.ToNSRange();
      NSURL* nsurl = net::NSURLWithGURL(link.url);
      if (nsurl) {
        NSDictionary* linkAttributes = @{
          NSLinkAttributeName : nsurl,
          NSFontAttributeName : PreferredFontForTextStyle(
              UIFontTextStyleCaption2, UIFontWeightSemibold)
        };
        [lineAttributed addAttributes:linkAttributes range:range];
      }
    }

    [result appendAttributedString:lineAttributed];
  }

  return result;
}

@end
