// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/fake_omnibox/fake_omnibox_view.h"

#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Line width for the fake omnibox.
constexpr CGFloat kLineWidth = 1.;
// Parameters for the fake omnibox.
constexpr CGFloat kFakeOmniboxCornerRadius = 99.;
// Parameters for empty field in the fake omnibox.
constexpr CGFloat kFakeOmniboxFieldWidth = 102.;
constexpr CGFloat kFakeOmniboxFieldHeight = 12.;
constexpr CGFloat kFakeOmniboxFieldCornerRadius = 12.;
// Magnifying glass size.
constexpr CGFloat kMagnifyingGlassSize = 20.;
constexpr CGFloat kMagnifyingGlassLeadingInset = 16.;
constexpr CGFloat kMagnifyingGlassTopInset = 12.;
// Font size for label
constexpr CGFloat kFontSize = 13.;

// Creates the fake omnibox border or shadow.
CALayer* CreateOmniboxEdgeLayer(BOOL isBorder) {
  CAShapeLayer* edgeLayer = [CAShapeLayer layer];
  if (isBorder) {
    // Create the dashed border line.
    edgeLayer.strokeColor = [UIColor colorNamed:kGrey300Color].CGColor;
    edgeLayer.fillColor = nil;
    edgeLayer.lineDashPattern = @[ @2, @1 ];

    edgeLayer.lineWidth = kLineWidth;
    edgeLayer.path =
        [UIBezierPath
            bezierPathWithRoundedRect:CGRectMake(0, 0, kFakeOmniboxWidth,
                                                 kFakeOmniboxHeight)
                         cornerRadius:kFakeOmniboxCornerRadius]
            .CGPath;
  } else {
    // Create the shadow
    edgeLayer.shadowColor = [UIColor colorNamed:kGrey300Color].CGColor;
    edgeLayer.shadowOpacity = 1;
    edgeLayer.shadowRadius = 16;
    edgeLayer.shadowOffset = CGSizeMake(0, 4);
    edgeLayer.shadowPath =
        [UIBezierPath
            bezierPathWithRoundedRect:CGRectMake(0, 0, kFakeOmniboxWidth,
                                                 kFakeOmniboxHeight)
                         cornerRadius:kFakeOmniboxCornerRadius]
            .CGPath;
  }
  return edgeLayer;
}

// Creates the fake omnibox field.
CALayer* CreateOmniboxFieldLayer(BOOL isEmpty) {
  CAShapeLayer* field = [CAShapeLayer layer];
  if (isEmpty) {
    // Add the empty grey field inside.
    field.fillColor = [UIColor colorNamed:kGrey100Color].CGColor;
    if (base::i18n::IsRTL()) {
      field.frame =
          CGRectMake(kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset -
                         kFakeOmniboxFieldWidth,
                     (kFakeOmniboxHeight - kFakeOmniboxFieldHeight) / 2.,
                     kFakeOmniboxFieldWidth, kFakeOmniboxFieldHeight);
    } else {
      field.frame =
          CGRectMake(kFakeOmniboxFieldLeadingInset,
                     (kFakeOmniboxHeight - kFakeOmniboxFieldHeight) / 2.,
                     kFakeOmniboxFieldWidth, kFakeOmniboxFieldHeight);
    }
    field.path =
        [UIBezierPath
            bezierPathWithRoundedRect:CGRectMake(0, 0, kFakeOmniboxFieldWidth,
                                                 kFakeOmniboxFieldHeight)
                         cornerRadius:kFakeOmniboxFieldCornerRadius]
            .CGPath;
    return field;
  }

  field.fillColor = [UIColor colorNamed:kBackgroundColor].CGColor;
  field.frame = CGRectMake(0, 0, kFakeOmniboxWidth, kFakeOmniboxHeight);
  field.path = [UIBezierPath
                   bezierPathWithRoundedRect:CGRectMake(0, 0, kFakeOmniboxWidth,
                                                        kFakeOmniboxHeight)
                                cornerRadius:kFakeOmniboxCornerRadius]
                   .CGPath;
  return field;
}

}  // namespace

@implementation FakeOmniboxView {
  // Omnibox edge layer with shadow and/or border line.
  CALayer* _omniboxEdgeLayer;
  // Inner layer.
  CALayer* _fieldLayer;
  // The image view for the favicon or the magnifying glass.
  UIImageView* _imageView;
  // Whether the fake omnibox should be empty.
  BOOL _isEmptyFakeOmnibox;
}

- (instancetype)initWithSearchEngineName:(NSString*)name
                            faviconImage:(UIImage*)image {
  self = [super initWithFrame:CGRectMake(0, 0, 0, 0)];
  if (self) {
    _isEmptyFakeOmnibox = !name && !image;

    self.bounds = CGRectMake(0, 0, kFakeOmniboxWidth, kFakeOmniboxHeight);

    // Add the shadow around the omnibox.
    _omniboxEdgeLayer = CreateOmniboxEdgeLayer(_isEmptyFakeOmnibox);
    [self.layer addSublayer:_omniboxEdgeLayer];

    // Create the pill-shaped field.
    _fieldLayer = CreateOmniboxFieldLayer(_isEmptyFakeOmnibox);
    [self.layer addSublayer:_fieldLayer];

    if (!_isEmptyFakeOmnibox) {
      // Add the search engine Label.
      UILabel* searchWithLabel = [[UILabel alloc] init];
      if (base::i18n::IsRTL()) {
        searchWithLabel.frame = CGRectMake(
            0., 0., kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset,
            kFakeOmniboxHeight);
      } else {
        searchWithLabel.frame =
            CGRectMake(kFakeOmniboxFieldLeadingInset, 0.,
                       kFakeOmniboxWidth - kFakeOmniboxFieldLeadingInset,
                       kFakeOmniboxHeight);
      }

      searchWithLabel.text =
          l10n_util::GetNSStringF(IDS_SEARCH_ENGINE_CHOICE_FAKE_OMNIBOX_TEXT,
                                  base::SysNSStringToUTF16(name));
      searchWithLabel.font = [UIFont systemFontOfSize:kFontSize];
      searchWithLabel.numberOfLines = 0;
      [self addSubview:searchWithLabel];
    }
    // Add the favicon on the side.
    _imageView = [[UIImageView alloc] initWithImage:_faviconImage];
    _imageView.layer.cornerRadius = kFaviconImageViewRadius;
    _imageView.clipsToBounds = YES;
    [self addSubview:_imageView];
    if (base::i18n::IsRTL()) {
      _imageView.frame =
          CGRectMake(kFakeOmniboxWidth - kMagnifyingGlassLeadingInset -
                         kFaviconImageViewSize,
                     kMagnifyingGlassTopInset, kFaviconImageViewSize,
                     kFaviconImageViewSize);
    } else {
      _imageView.frame =
          CGRectMake(kMagnifyingGlassLeadingInset, kMagnifyingGlassTopInset,
                     kFaviconImageViewSize, kFaviconImageViewSize);
    }

    if (_isEmptyFakeOmnibox) {
      _imageView.image = DefaultSymbolWithPointSize(kMagnifyingglassSymbol,
                                                    kMagnifyingGlassSize);
    } else {
      self.faviconImage = image;
      _imageView.image = image;
    }
  }
  return self;
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    // Creates new layers, to have the correct colors (according to dark mode or
    // light mode).
    CALayer* newOmniboxEdgeLayer = CreateOmniboxEdgeLayer(_isEmptyFakeOmnibox);
    [self.layer replaceSublayer:_omniboxEdgeLayer with:newOmniboxEdgeLayer];
    _omniboxEdgeLayer = newOmniboxEdgeLayer;
    CALayer* newFieldLayer = CreateOmniboxFieldLayer(_isEmptyFakeOmnibox);
    [self.layer replaceSublayer:_fieldLayer with:newFieldLayer];
    _fieldLayer = newFieldLayer;
  }
}

#pragma mark - Properties

- (void)setFaviconImage:(UIImage*)faviconImage {
  CHECK(!_isEmptyFakeOmnibox);
  _faviconImage = faviconImage;
  _imageView.image = faviconImage;
}

@end
