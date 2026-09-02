// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"

#import <QuartzCore/QuartzCore.h>

#import <algorithm>

#import "base/debug/dump_without_crashing.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view_utils.h"

namespace {

constexpr CGFloat kWatermarkBlockSpacing = 40.0;

// The rotation angle for attribution watermarks.
constexpr CGFloat kRotationAngle = -M_PI_4;  // -45 degrees
constexpr CGFloat kWatermarkBlockWidthToFontSizeRatio = 10.0;

// Creates and configures a label with standard watermark styling parameters.
UILabel* CreateWatermarkLabel() {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.textAlignment = NSTextAlignmentLeft;
  return label;
}

}  // namespace

// A helper view that is backed by a `CAReplicatorLayer`.
@interface WatermarkReplicatorView : UIView
@end

@implementation WatermarkReplicatorView
+ (Class)layerClass {
  return [CAReplicatorLayer class];
}
@end

@implementation WatermarkView {
  // The horizontal replicators that stamp the watermark repeatedly along the
  // x-axis. Two separate replicators are required so that even and odd numbered
  // rows can be configured with different layout parameters (stagger/shift
  // offsets) before being paired and duplicated vertically. This is part of the
  // UI requirement that odd numbered rows should shift to the right with
  // `kWatermarkBlockSpacing` + x offset.
  WatermarkReplicatorView* _hReplicatorEven;
  WatermarkReplicatorView* _hReplicatorOdd;

  // The vertical replicator that duplicates the row pairs along the y-axis.
  WatermarkReplicatorView* _vReplicator;

  // The original, single text watermark "stamps" for even and odd rows.
  // Two separate labels are required because a UIView can only have a single
  // superview at any given time, preventing the same label instance from being
  // added to both horizontal replicators.
  UILabel* _labelEven;
  UILabel* _labelOdd;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUpView];
  }
  return self;
}

// Configures internal view properties, initializes subviews, and sets up the
// nested replicator view hierarchy.
- (void)setUpView {
  self.backgroundColor = [UIColor clearColor];
  self.userInteractionEnabled = NO;
  self.layer.zPosition = CGFLOAT_MAX;
  self.clipsToBounds = YES;

  _vReplicator = [[WatermarkReplicatorView alloc] initWithFrame:CGRectZero];
  _hReplicatorEven = [[WatermarkReplicatorView alloc] initWithFrame:CGRectZero];
  _hReplicatorOdd = [[WatermarkReplicatorView alloc] initWithFrame:CGRectZero];

  _labelEven = CreateWatermarkLabel();
  _labelOdd = CreateWatermarkLabel();

  // Set up the nested replicator hierarchy to create the staggered grid:
  // 1. Assign even and odd base labels to their respective horizontal
  // replicators.
  //    Two separate labels are required because a UIView can only have a single
  //    superview at any time.
  // 2. Add both horizontal replicators as subviews of the vertical replicator.
  //    This groups them into a single "double-row" unit so they can be
  //    duplicated together vertically.
  // 3. Add the vertical replicator to the main view to fill the screen area.
  [_hReplicatorEven addSubview:_labelEven];
  [_hReplicatorOdd addSubview:_labelOdd];
  [_vReplicator addSubview:_hReplicatorEven];
  [_vReplicator addSubview:_hReplicatorOdd];
  [self addSubview:_vReplicator];

  // Mobile-tuned default watermark style values.
  _fillOpacity = 0.10;     // 10%
  _outlineOpacity = 0.15;  // 15%
  _fontSize = 16.0;
}

- (void)setText:(NSString*)text {
  if ([_text isEqualToString:text]) {
    return;
  }
  _text = [text copy];
  [self updateLabel];
}

- (void)setFillOpacity:(CGFloat)fillOpacity {
  if (fillOpacity < 0.0 || fillOpacity > 1.0) {
    base::debug::DumpWithoutCrashing();
  }

  CGFloat clampedFillOpacity = std::clamp<CGFloat>(fillOpacity, 0.0, 1.0);
  if (_fillOpacity == clampedFillOpacity) {
    return;
  }
  _fillOpacity = clampedFillOpacity;
  [self updateLabel];
}

- (void)setOutlineOpacity:(CGFloat)outlineOpacity {
  if (outlineOpacity < 0.0 || outlineOpacity > 1.0) {
    base::debug::DumpWithoutCrashing();
  }
  CGFloat clampedOutlineOpacity = std::clamp<CGFloat>(outlineOpacity, 0.0, 1.0);
  if (_outlineOpacity == clampedOutlineOpacity) {
    return;
  }
  _outlineOpacity = clampedOutlineOpacity;
  [self updateLabel];
}

- (void)setFontSize:(CGFloat)fontSize {
  if (_fontSize == fontSize) {
    return;
  }
  _fontSize = fontSize;
  [self updateLabel];
}

#pragma mark - Private

// Re-creates and applies the high-contrast attributed text to both watermark
// labels using current styling properties, then requests a new layout pass.
- (void)updateLabel {
  if (_text.length == 0) {
    _labelEven.attributedText = nil;
    _labelOdd.attributedText = nil;
    [self setNeedsLayout];
    return;
  }

  // We use a fixed High-Contrast strategy (Black text with a White halo) to
  // guarantee visibility on all content types (Images, PDFs, Dark/Light pages)
  // regardless of the system theme.
  UIColor* fillColor = [UIColor colorWithWhite:0.0 alpha:_fillOpacity];
  UIColor* outlineColor = [UIColor colorWithWhite:1.0 alpha:_outlineOpacity];
  NSDictionary* attributes = @{
    NSFontAttributeName : [UIFont systemFontOfSize:_fontSize
                                            weight:UIFontWeightSemibold],
    NSForegroundColorAttributeName : fillColor,
    NSStrokeColorAttributeName : outlineColor,

    // Negative value means fill and stroke the text, and -2.0 creates a strong
    // halo.
    NSStrokeWidthAttributeName : @-2.0,
  };

  NSAttributedString* attrText =
      [[NSAttributedString alloc] initWithString:_text attributes:attributes];
  _labelEven.attributedText = attrText;
  _labelOdd.attributedText = attrText;
  [self setNeedsLayout];
}

// Measures and lays out the single watermark label tile.
//
// @return The `CGSize` representing the frame size of the watermark label.
- (CGSize)layoutWatermarkLabelTile {
  CGFloat blockWidth = self.fontSize * kWatermarkBlockWidthToFontSizeRatio;
  CGSize intrinsicSize =
      [_labelEven sizeThatFits:CGSizeMake(blockWidth, CGFLOAT_MAX)];
  CGRect labelFrame =
      CGRectMake(0, 0, ceil(intrinsicSize.width), ceil(intrinsicSize.height));
  _labelEven.frame = labelFrame;
  _labelOdd.frame = labelFrame;
  return labelFrame.size;
}

// Configures and positions the vertical replicator (the grid) with the given
// layout parameters.
//
// @param expandedSize The size of the vertical replicator view, scaled to cover
//   the visible area after rotation.
// @param verticalTileInterval The vertical distance between consecutive rows
//   (label height + spacing).
// @param rowCount The number of vertical replicator instances (row-pairs) to
//   cover the expanded height.
// @param angle The rotation angle in radians applied to the vertical replicator
//   view.
- (void)configureVerticalReplicatorWithExpandedSize:(CGSize)expandedSize
                               verticalTileInterval:
                                   (CGFloat)verticalTileInterval
                                           rowCount:(NSInteger)rowCount
                                              angle:(CGFloat)angle {
  // Reset the transform to identity before modifying geometric properties
  // (bounds and center) to ensure calculations are based on an unrotated,
  // axis-aligned coordinate system and to prevent layout side-effects across
  // multiple passes.
  _vReplicator.transform = CGAffineTransformIdentity;
  _vReplicator.bounds =
      CGRectMake(0, 0, expandedSize.width, expandedSize.height);
  _vReplicator.center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  _vReplicator.transform = CGAffineTransformMakeRotation(angle);

  CAReplicatorLayer* vLayer = (CAReplicatorLayer*)_vReplicator.layer;
  vLayer.instanceCount = rowCount;

  // Translates each subsequent instance vertically by twice the vertical tile
  // interval. This is because each instance of the vertical replicator contains
  // both the even row (at Y = 0) and the odd row (at Y = verticalTileInterval)
  // as a paired unit.
  vLayer.instanceTransform =
      CATransform3DMakeTranslation(0, 2.0 * verticalTileInterval, 0);
}

// Configures and positions the horizontal replicators (even and odd rows) with
// the given layout parameters.
//
// @param expandedWidth The target width to fill with replicated watermark
// tiles.
// @param staggerOffset The horizontal translation applied to the odd row
//   relative to the even row to create an interleaved/staggered visual layout.
// @param horizontalTileInterval The horizontal distance between the start of
//   consecutive tiles in a row (label width + spacing).
// @param verticalTileInterval The vertical offset applied to the odd row
//   (label height + spacing).
- (void)configureHorizontalReplicatorsWithExpandedWidth:(CGFloat)expandedWidth
                                          staggerOffset:(CGFloat)staggerOffset
                                 horizontalTileInterval:
                                     (CGFloat)horizontalTileInterval
                                   verticalTileInterval:
                                       (CGFloat)verticalTileInterval {
  // Even row has no stagger offset.
  _hReplicatorEven.frame =
      CGRectMake(0, 0, expandedWidth, _labelEven.frame.size.height);
  CAReplicatorLayer* evenLayer = (CAReplicatorLayer*)_hReplicatorEven.layer;
  evenLayer.instanceCount = ceil(expandedWidth / horizontalTileInterval) + 1;
  evenLayer.instanceTransform =
      CATransform3DMakeTranslation(horizontalTileInterval, 0, 0);

  // Odd row is shifted horizontally by `staggerOffset` and vertically by
  // `verticalTileInterval`.
  _hReplicatorOdd.frame =
      CGRectMake(-staggerOffset, verticalTileInterval,
                 expandedWidth + staggerOffset, _labelOdd.frame.size.height);
  CAReplicatorLayer* oddLayer = (CAReplicatorLayer*)_hReplicatorOdd.layer;
  oddLayer.instanceCount =
      ceil((expandedWidth + staggerOffset) / horizontalTileInterval) + 1;
  oddLayer.instanceTransform =
      CATransform3DMakeTranslation(horizontalTileInterval, 0, 0);
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  if (self.text.length == 0) {
    _vReplicator.hidden = YES;
    return;
  }
  _vReplicator.hidden = NO;

  // Calculate and layout the single watermark label tiles.
  CGSize tileSize = [self layoutWatermarkLabelTile];
  CGFloat horizontalTileInterval = tileSize.width + kWatermarkBlockSpacing;
  CGFloat verticalTileInterval = tileSize.height + kWatermarkBlockSpacing;
  CGFloat angle = kRotationAngle;

  // Calculate the larger bounding size required for the watermark grid after
  // rotating `angle` degrees.
  CGSize expandedSize =
      GetWatermarkExpandedSizeForRotation(self.bounds.size, angle);

  // Configure the Horizontal Replicators (Even & Odd rows)
  CGFloat staggerOffset = horizontalTileInterval / 2.0;
  [self configureHorizontalReplicatorsWithExpandedWidth:expandedSize.width
                                          staggerOffset:staggerOffset
                                 horizontalTileInterval:horizontalTileInterval
                                   verticalTileInterval:verticalTileInterval];

  // The odd and even horizontal replicators are grouped as a single subview
  // unit inside `_vReplicator`. Therefore, a single vertical replication step
  // duplicates both rows together, translating this entire pair down by 2 *
  // `verticalTileInterval` along the Y-axis to form the interlocking grid.
  // Add an extra pair of rows to guarantee full coverage of boundaries on top
  // and bottom.
  expandedSize.height += 2.0 * verticalTileInterval;
  NSInteger pairCount =
      ceil(expandedSize.height / (2.0 * verticalTileInterval));

  // Align the replicator bounds exactly with the total height covered by the
  // instances.
  expandedSize.height = pairCount * 2.0 * verticalTileInterval;
  [self configureVerticalReplicatorWithExpandedSize:expandedSize
                               verticalTileInterval:verticalTileInterval
                                           rowCount:pairCount
                                              angle:angle];
}

#pragma mark - WatermarkConsumer

- (void)updateWatermarkWithText:(NSString*)text style:(WatermarkStyle)style {
  self.text = text;
  if (style.fill_opacity.has_value()) {
    self.fillOpacity = style.fill_opacity.value();
  }
  if (style.outline_opacity.has_value()) {
    self.outlineOpacity = style.outline_opacity.value();
  }
  if (style.font_size.has_value()) {
    self.fontSize = style.font_size.value();
  }
}

@end
