// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_filter_cell.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
namespace {

// Constants for download filter cell layout.
const CGFloat kDownloadFilterCellHeight = 32.0;
const CGFloat kDownloadFilterIconTitleSpacing = 6.0;
const CGFloat kDownloadFilterCellHorizontalPadding = 12.0;
const CGFloat kDownloadFilterIconSize = 20.0;

// Font style for download filter cell title.
UIFontTextStyle GetDownloadFilterCellTitleFontStyle() {
  return UIFontTextStyleFootnote;
}

// Returns the localized display text for the given filter type.
NSString* GetFilterTypeDisplayText(DownloadFilterType filterType) {
  switch (filterType) {
    case DownloadFilterType::kAll:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_ALL);
    case DownloadFilterType::kDocument:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_DOCUMENTS);
    case DownloadFilterType::kImage:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_IMAGES);
    case DownloadFilterType::kVideo:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_VIDEOS);
    case DownloadFilterType::kAudio:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_AUDIOS);
    case DownloadFilterType::kPDF:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_PDFS);
    case DownloadFilterType::kOther:
      return l10n_util::GetNSString(IDS_IOS_DOWNLOAD_FILTER_OTHERS);
  }
}

// Returns the corresponding SF Symbol name for the given filter type.
NSString* GetFilterTypeSymbolName(DownloadFilterType filterType) {
  switch (filterType) {
    case DownloadFilterType::kAll:
      return kCheckmarkSymbol;
    case DownloadFilterType::kDocument:
      return kTextJustifyLeftSymbol;
    case DownloadFilterType::kImage:
      return kPhotoSymbol;
    case DownloadFilterType::kVideo:
      return kVideoSymbol;
    case DownloadFilterType::kAudio:
      return kWaveformSymbol;
    case DownloadFilterType::kPDF:
      return kTextDocument;
    case DownloadFilterType::kOther:
      return kDocSymbol;
  }
}

}  // namespace

@implementation DownloadFilterCell {
  // Title label displaying the filter type name.
  UILabel* _titleLabel;

  // Image view displaying the filter type icon.
  UIImageView* _iconImageView;

  // Current filter type for this cell.
  DownloadFilterType _filterType;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setupViews];
  }
  return self;
}

- (void)setupViews {
  self.layer.cornerRadius = kDownloadFilterCellHeight / 2;

  _iconImageView = [[UIImageView alloc] init];
  _iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _iconImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [self.contentView addSubview:_iconImageView];

  // Setup title label.
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.font =
      [UIFont preferredFontForTextStyle:GetDownloadFilterCellTitleFontStyle()];
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _titleLabel.textAlignment = NSTextAlignmentCenter;
  [self.contentView addSubview:_titleLabel];

  // Setup constraints.
  [NSLayoutConstraint activateConstraints:@[
    // Icon constraints.
    [_iconImageView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kDownloadFilterCellHorizontalPadding],
    [_iconImageView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [_iconImageView.widthAnchor
        constraintEqualToConstant:kDownloadFilterIconSize],
    [_iconImageView.heightAnchor
        constraintEqualToConstant:kDownloadFilterIconSize],

    // Title label constraints.
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_iconImageView.trailingAnchor
                       constant:kDownloadFilterIconTitleSpacing],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kDownloadFilterCellHorizontalPadding],
    [_titleLabel.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
  ]];
}

- (void)configureWithFilterType:(DownloadFilterType)filterType {
  _filterType = filterType;
  _titleLabel.text = GetFilterTypeDisplayText(filterType);
  _iconImageView.image = DefaultSymbolWithPointSize(
      GetFilterTypeSymbolName(filterType), kDownloadFilterIconSize);
}

- (void)setSelected:(BOOL)selected {
  [super setSelected:selected];
  if (selected) {
    self.backgroundColor = [UIColor colorNamed:kBlueColor];
    _titleLabel.textColor = [UIColor colorNamed:kInvertedTextPrimaryColor];
    _iconImageView.tintColor = [UIColor colorNamed:kInvertedTextPrimaryColor];
    self.layer.borderWidth = 0.0;
    self.layer.borderColor = [UIColor clearColor].CGColor;
  } else {
    self.backgroundColor = UIColor.clearColor;
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _iconImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    self.layer.borderWidth = 1.0;
    self.layer.borderColor = [UIColor colorNamed:kTextQuaternaryColor].CGColor;
  }
}

+ (CGFloat)cellSizeForFilterType:(DownloadFilterType)filterType {
  NSString* text = GetFilterTypeDisplayText(filterType);

  UIFont* font =
      [UIFont preferredFontForTextStyle:GetDownloadFilterCellTitleFontStyle()];

  // Calculate the actual text size using the cell's font to ensure proper fit.
  CGSize textSize =
      [text boundingRectWithSize:CGSizeMake(CGFLOAT_MAX,
                                            kDownloadFilterCellHeight)
                         options:NSStringDrawingUsesLineFragmentOrigin
                      attributes:@{NSFontAttributeName : font}
                         context:nil]
          .size;

  // Calculate total width including icon, spacing, and padding on both sides.
  CGFloat horizontalPadding = kDownloadFilterCellHorizontalPadding * 2;
  CGFloat cellWidth = ceil(textSize.width) + kDownloadFilterIconSize +
                      kDownloadFilterIconTitleSpacing + horizontalPadding;

  return cellWidth;
}

+ (CGFloat)cellHeight {
  return kDownloadFilterCellHeight;
}

@end
