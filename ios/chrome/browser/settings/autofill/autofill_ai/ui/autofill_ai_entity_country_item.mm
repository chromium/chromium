// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_country_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// The size of trailing symbol icons.
constexpr NSInteger kTrailingSymbolImagePointSize = 18;

}  // namespace

@implementation AutofillAIEntityCountryItem

@synthesize attributeType = _attributeType;
@synthesize hasValidValueStatus = _hasValidValueStatus;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  TableViewCellContentConfiguration* contentConfig =
      base::apple::ObjCCast<TableViewCellContentConfiguration>(
          cell.contentConfiguration);
  if (!contentConfig) {
    return;
  }

  contentConfig.hasAccessoryView = YES;
  contentConfig.trailingTextColor = [UIColor colorNamed:kTextPrimaryColor];

  NSString* text = self.detailText;
  if (!self.hasValidValueStatus) {
    contentConfig.attributedTrailingText = [self attributedErrorText:text];
  } else {
    contentConfig.attributedTrailingText = nil;
    contentConfig.trailingText = text;
  }

  cell.contentConfiguration = contentConfig;
}

#pragma mark - Private

// Returns an attributed string for the error state.
- (NSAttributedString*)attributedErrorText:(NSString*)text {
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text];

  UIImage* image = [DefaultSymbolWithPointSize(kErrorCircleFillSymbol,
                                               kTrailingSymbolImagePointSize)
      imageWithTintColor:[UIColor colorNamed:kRedColor]];

  UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  CGFloat yOffset = (font.capHeight - image.size.height) / 2.0;

  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = image;
  attachment.bounds =
      CGRectMake(0, yOffset, image.size.width, image.size.height);

  [attributedText
      appendAttributedString:[[NSAttributedString alloc] initWithString:@" "]];
  [attributedText
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:attachment]];

  return attributedText;
}

@end
