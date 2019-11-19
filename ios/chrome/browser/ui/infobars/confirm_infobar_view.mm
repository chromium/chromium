// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/confirm_infobar_view.h"

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>

#include "base/format_macros.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/ios/uikit_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kChromeInfobarURL[] = "chromeinternal://infobar/";

// UX configuration metrics for the layout of items.
struct LayoutMetrics {
  CGFloat left_margin_on_first_line_when_icon_absent;
  CGFloat minimum_space_between_right_and_left_aligned_widgets;
  CGFloat right_margin;
  CGFloat space_between_widgets;
  CGFloat close_button_inner_padding;
  CGFloat button_height;
  CGFloat button_margin;
  CGFloat extra_button_margin_on_single_line;
  CGFloat button_spacing;
  CGFloat button_width_units;
  CGFloat buttons_margin_top;
  CGFloat close_button_margin_left;
  CGFloat label_line_spacing;
  CGFloat label_margin_bottom;
  CGFloat extra_margin_between_label_and_button;
  CGFloat label_margin_top;
  CGFloat minimum_infobar_height;
  CGFloat horizontal_space_between_icon_and_text;
};

const LayoutMetrics kLayoutMetrics = {
    20.0,  // left_margin_on_first_line_when_icon_absent
    30.0,  // minimum_space_between_right_and_left_aligned_widgets
    10.0,  // right_margin
    10.0,  // space_between_widgets
    8.0,   // close_button_inner_padding
    36.0,  // button_height
    16.0,  // button_margin
    8.0,   // extra_button_margin_on_single_line
    8.0,   // button_spacing
    8.0,   // button_width_units
    16.0,  // buttons_margin_top
    16.0,  // close_button_margin_left
    5.0,   // label_line_spacing
    22.0,  // label_margin_bottom
    0.0,   // extra_margin_between_label_and_button
    21.0,  // label_margin_top
    68.0,  // minimum_infobar_height
    16.0   // horizontal_space_between_icon_and_text
};

// Corner radius for action buttons.
const CGFloat kButtonCornerRadius = 8.0;

enum InfoBarButtonPosition { ON_FIRST_LINE, CENTER, LEFT, RIGHT };

// Returns the font for the Infobar's main body text.
UIFont* InfoBarLabelFont() {
  // Due to https://crbug.com/989761, disable dynamic type. Once migration to
  // Messages is complete, this class will be deleted.
  if (@available(iOS 13, *)) {
    return [UIFont systemFontOfSize:17];
  } else {
    return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  }
}

// Returns the font for the Infobar's toggle switch's (if one exists) body text.
UIFont* InfoBarSwitchLabelFont() {
  // Due to https://crbug.com/989761, disable dynamic type. Once migration to
  // Messages is complete, this class will be deleted.
  if (@available(iOS 13, *)) {
    return [UIFont systemFontOfSize:17];
  } else {
    return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  }
}

// Returns the font for the label on Infobar's action buttons.
UIFont* InfoBarButtonLabelFont() {
  // Due to https://crbug.com/989761, disable dynamic type. Once migration to
  // Messages is complete, this class will be deleted.
  if (@available(iOS 13, *)) {
    return [UIFont systemFontOfSize:17 weight:UIFontWeightSemibold];
  } else {
    return [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  }
}

UIImage* InfoBarCloseImage() {
  ui::ResourceBundle& resourceBundle = ui::ResourceBundle::GetSharedInstance();
  return resourceBundle.GetNativeImageNamed(IDR_IOS_INFOBAR_CLOSE).ToUIImage();
}

}  // namespace

// UIView containing a label.
@interface InfobarFooterView : BidiContainerView

@property(nonatomic, readonly) UILabel* label;
@property(nonatomic) CGFloat preferredLabelWidth;

// Initialize the view's label with |labelText|.
- (instancetype)initWithText:(NSString*)labelText NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the height taken by the view constrained by a width of |width|.
// If |layout| is yes, it sets the frame of the view to fit |width|.
- (CGFloat)heightRequiredForFooterWithWidth:(CGFloat)width layout:(BOOL)layout;

// Returns the preferred width. A smaller width requires eliding the text.
- (CGFloat)preferredWidth;
@end

@implementation InfobarFooterView

- (instancetype)initWithText:(NSString*)labelText {
  // Creates label.
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.textAlignment = NSTextAlignmentNatural;
  label.font = InfoBarSwitchLabelFont();
  label.text = labelText;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.backgroundColor = UIColor.clearColor;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.numberOfLines = 0;
  label.adjustsFontSizeToFitWidth = NO;
  [label sizeToFit];

  self = [super initWithFrame:label.frame];
  if (!self)
    return nil;
  _label = label;
  _preferredLabelWidth = CGRectGetMaxX(_label.frame);
  [self addSubview:_label];
  return self;
}

- (CGFloat)heightRequiredForFooterWithWidth:(CGFloat)width layout:(BOOL)layout {
  CGFloat widthLeftForLabel = width;
  CGSize maxSize = CGSizeMake(widthLeftForLabel, CGFLOAT_MAX);
  CGSize labelSize =
      [[self.label text] cr_boundingSizeWithSize:maxSize
                                            font:[self.label font]];
  CGFloat viewHeight = labelSize.height;
  if (layout) {
    // Lays out the label and the switch to fit in {width, viewHeight}.
    CGRect newLabelFrame = CGRectMake(0, 0, labelSize.width, labelSize.height);
    newLabelFrame = AlignRectOriginAndSizeToPixels(newLabelFrame);
    [self.label setFrame:newLabelFrame];
  }
  return viewHeight;
}

- (CGFloat)preferredWidth {
  return self.preferredLabelWidth;
}

@end

// UIView containing a switch and a label.
@interface SwitchView : InfobarFooterView

// Initialize the view's label with |labelText|.
- (instancetype)initWithText:(NSString*)labelText
                        isOn:(BOOL)isOn NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithText:(NSString*)labelText NS_UNAVAILABLE;

// Specifies the object, action, and tag used when the switch is toggled.
- (void)setTag:(NSInteger)tag target:(id)target action:(SEL)action;

// Returns the height taken by the view constrained by a width of |width|.
// If |layout| is yes, it sets the frame of the label and the switch to fit
// |width|.
- (CGFloat)heightRequiredForFooterWithWidth:(CGFloat)width layout:(BOOL)layout;

// Returns the preferred width. A smaller width requires eliding the text.
- (CGFloat)preferredWidth;

@end

@implementation SwitchView {
  UISwitch* _switch;
  CGFloat _preferredTotalWidth;
  // Layout metrics for calculating item placement.
  const LayoutMetrics* _metrics;
}

- (instancetype)initWithText:(NSString*)labelText isOn:(BOOL)isOn {
  _metrics = &kLayoutMetrics;

  self = [super initWithText:labelText];
  if (!self)
    return nil;

  self.label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _switch = [[UISwitch alloc] initWithFrame:CGRectZero];
  _switch.exclusiveTouch = YES;
  _switch.accessibilityLabel = labelText;
  _switch.onTintColor = [UIColor colorNamed:kBlueColor];
  _switch.on = isOn;

  // Computes the size and initializes the view.
  CGSize labelSize = self.label.frame.size;
  CGSize switchSize = _switch.frame.size;
  CGRect frameRect = CGRectMake(
      0, 0,
      labelSize.width + _metrics->space_between_widgets + switchSize.width,
      std::max(labelSize.height, switchSize.height));
  [self setFrame:frameRect];

  // Sets the position of the label and the switch. The label is left aligned
  // and the switch is right aligned. Both are vertically centered.
  CGRect labelFrame =
      CGRectMake(0, (self.frame.size.height - labelSize.height) / 2,
                 labelSize.width, labelSize.height);
  CGRect switchFrame =
      CGRectMake(self.frame.size.width - switchSize.width,
                 (self.frame.size.height - switchSize.height) / 2,
                 switchSize.width, switchSize.height);

  labelFrame = AlignRectOriginAndSizeToPixels(labelFrame);
  switchFrame = AlignRectOriginAndSizeToPixels(switchFrame);

  [self.label setFrame:labelFrame];
  [_switch setFrame:switchFrame];
  _preferredTotalWidth = CGRectGetMaxX(switchFrame);
  self.preferredLabelWidth = CGRectGetMaxX(labelFrame);

  [self addSubview:self.label];
  [self addSubview:_switch];
  return self;
}

- (void)setTag:(NSInteger)tag target:(id)target action:(SEL)action {
  [_switch setTag:tag];
  [_switch addTarget:target
                action:action
      forControlEvents:UIControlEventValueChanged];
}

- (CGFloat)heightRequiredForFooterWithWidth:(CGFloat)width layout:(BOOL)layout {
  CGFloat widthLeftForLabel =
      width - [_switch frame].size.width - _metrics->space_between_widgets;
  CGSize maxSize = CGSizeMake(widthLeftForLabel, CGFLOAT_MAX);
  CGSize labelSize =
      [[self.label text] cr_boundingSizeWithSize:maxSize
                                            font:[self.label font]];
  CGFloat viewHeight = std::max(labelSize.height, [_switch frame].size.height);
  if (layout) {
    // Lays out the label and the switch to fit in {width, viewHeight}.
    CGRect newLabelFrame;
    newLabelFrame.origin.x = 0;
    newLabelFrame.origin.y = (viewHeight - labelSize.height) / 2;
    newLabelFrame.size = labelSize;
    newLabelFrame = AlignRectOriginAndSizeToPixels(newLabelFrame);
    [self.label setFrame:newLabelFrame];
    CGRect newSwitchFrame;
    newSwitchFrame.origin.x =
        CGRectGetMaxX(newLabelFrame) + _metrics->space_between_widgets;
    newSwitchFrame.origin.y = (viewHeight - [_switch frame].size.height) / 2;
    newSwitchFrame.size = [_switch frame].size;
    newSwitchFrame = AlignRectOriginAndSizeToPixels(newSwitchFrame);
    [_switch setFrame:newSwitchFrame];
  }
  return viewHeight;
}

- (CGFloat)preferredWidth {
  return _preferredTotalWidth;
}

@end

@interface ConfirmInfoBarView (Testing)
// Returns the buttons' height.
- (CGFloat)buttonsHeight;
// Returns the button margin applied in some views.
- (CGFloat)buttonMargin;
// Returns the height of the infobar, and lays out the subviews if |layout| is
// YES.
- (CGFloat)computeRequiredHeightAndLayoutSubviews:(BOOL)layout;
// Returns the height of the laid out buttons when not on the first line.
// Either the buttons are narrow enough and they are on a single line next to
// each other, or they are supperposed on top of each other.
// Also lays out the buttons when |layout| is YES, in which case it uses
// |heightOfFirstLine| to compute their vertical position.
- (CGFloat)heightThatFitsButtonsUnderOtherWidgets:(CGFloat)heightOfFirstLine
                                           layout:(BOOL)layout;
// The |button| is positioned with the right edge at the specified y-axis
// position |rightEdge| and the top row at |y|.
// Returns the left edge of the newly-positioned button.
- (CGFloat)layoutWideButtonAlignRight:(UIButton*)button
                            rightEdge:(CGFloat)rightEdge
                                    y:(CGFloat)y;
// Returns the minimum height of infobars.
- (CGFloat)minimumInfobarHeight;
// Returns |string| stripped of the markers specifying the links and fills
// |linkRanges_| with the ranges of the enclosed links.
- (NSString*)stripMarkersFromString:(NSString*)string;
// Returns the ranges of the links and the associated tags.
- (const std::vector<std::pair<NSUInteger, NSRange>>&)linkRanges;
@end

@interface ConfirmInfoBarView ()

// Returns the marker delimiting the start of a link.
+ (NSString*)openingMarkerForLink;
// Returns the marker delimiting the end of a link.
+ (NSString*)closingMarkerForLink;

// How much of the infobar (in points) is visible (e.g., during showing/hiding
// animation).
@property(nonatomic, assign) CGFloat visibleHeight;

// Separator above the view separating it from the web content.
@property(nonatomic, strong) UIView* separator;

@end

@implementation ConfirmInfoBarView {
  // The height of this infobar when fully visible.
  CGFloat targetHeight_;
  // View containing the icon.
  UIImageView* imageView_;
  // Close button.
  UIButton* closeButton_;
  // View containing the label and maybe switch.
  InfobarFooterView* InfobarFooterView_;
  // We are using a LabelLinkController with an UILabel to be able to have
  // parts of the label underlined and clickable. This label_ may be nil if
  // the delegate returns an empty string for GetMessageText().
  LabelLinkController* labelLinkController_;
  UILabel* label_;
  // Array of range information. The first element of the pair is the tag of
  // the action and the second element is the range defining the link.
  std::vector<std::pair<NSUInteger, NSRange>> linkRanges_;
  // Text for the label with link markers included.
  NSString* markedLabel_;
  // Buttons.
  // button1_ is tagged with ConfirmInfoBarDelegate::BUTTON_OK .
  // button2_ is tagged with ConfirmInfoBarDelegate::BUTTON_CANCEL .
  UIButton* button1_;
  UIButton* button2_;
  // Drop shadow.
  UIImageView* shadow_;
  // Layout metrics for calculating item placement.
  const LayoutMetrics* metrics_;
}

@synthesize visibleHeight = visibleHeight_;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    metrics_ = &kLayoutMetrics;
    [self setAccessibilityViewIsModal:YES];

    // Add a separator above the view separating it from the web content.
    _separator = [[UIView alloc] init];
    _separator.translatesAutoresizingMaskIntoConstraints = NO;
    _separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  }
  return self;
}

- (NSString*)markedLabel {
  return markedLabel_;
}

// Returns the width reserved for the icon.
- (CGFloat)leftMarginOnFirstLine {
  CGFloat leftMargin = 0;
  if (imageView_) {
    leftMargin += CGRectGetMaxX([self frameOfIcon]);
    leftMargin += metrics_->horizontal_space_between_icon_and_text;
  } else {
    leftMargin += metrics_->left_margin_on_first_line_when_icon_absent;
    leftMargin += self.safeAreaInsets.left;
  }
  return leftMargin;
}

// Returns the width reserved for the close button.
- (CGFloat)rightMarginOnFirstLine {
  return [closeButton_ imageView].image.size.width +
         metrics_->close_button_inner_padding * 2 + self.safeAreaInsets.right;
}

// Returns the horizontal space available between the icon and the close
// button.
- (CGFloat)horizontalSpaceAvailableOnFirstLine {
  return [self frame].size.width - [self leftMarginOnFirstLine] -
         [self rightMarginOnFirstLine];
}

// Returns the height taken by a label constrained by a width of |width|.
- (CGFloat)heightRequiredForLabelWithWidth:(CGFloat)width {
  return [label_ sizeThatFits:CGSizeMake(width, CGFLOAT_MAX)].height;
}

// Returns the width required by a label if it was displayed on a single line.
- (CGFloat)widthOfLabelOnASingleLine {
  // |label_| can be nil when delegate returns "" for GetMessageText().
  if (!label_)
    return 0.0;
  CGSize rect = [[label_ text] cr_pixelAlignedSizeWithFont:[label_ font]];
  return rect.width;
}

// Returns the minimum size required by |button| to be properly displayed.
- (CGFloat)narrowestWidthOfButton:(UIButton*)button {
  if (!button)
    return 0;
  // The button itself is queried for the size. The width is rounded up to be a
  // multiple of 8 to fit Material grid spacing requirements.
  CGFloat labelWidth =
      [button sizeThatFits:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)].width;
  return ceil(labelWidth / metrics_->button_width_units) *
         metrics_->button_width_units;
}

// Returns the width of the buttons if they are laid out on the first line.
- (CGFloat)widthOfButtonsOnFirstLine {
  CGFloat width = [self narrowestWidthOfButton:button1_] +
                  [self narrowestWidthOfButton:button2_];
  if (button1_ && button2_) {
    width += metrics_->space_between_widgets;
  }
  return width;
}

// Returns the width needed for the switch.
- (CGFloat)preferredWidthOfSwitch {
  return [InfobarFooterView_ preferredWidth];
}

// Returns the space required to separate the left aligned widgets (label) from
// the right aligned widgets (switch, buttons), assuming they fit on one line.
- (CGFloat)widthToSeparateRightAndLeftWidgets {
  BOOL leftWidgetsArePresent = (label_ != nil);
  BOOL rightWidgetsArePresent = button1_ || button2_ || InfobarFooterView_;
  if (!leftWidgetsArePresent || !rightWidgetsArePresent)
    return 0;
  return metrics_->minimum_space_between_right_and_left_aligned_widgets;
}

// Returns the space required to separate the switch and the buttons.
- (CGFloat)widthToSeparateSwitchAndButtons {
  BOOL buttonsArePresent = button1_ || button2_;
  BOOL switchIsPresent = (InfobarFooterView_ != nil);
  if (!buttonsArePresent || !switchIsPresent)
    return 0;
  return metrics_->space_between_widgets;
}

// Lays out |button| at the height |y| and in the position |position|.
// Must only be used for wide buttons, i.e. buttons not on the first line.
- (void)layoutWideButton:(UIButton*)button
                       y:(CGFloat)y
                position:(InfoBarButtonPosition)position {
  CGFloat screenWidth = [self frame].size.width;
  CGFloat startPercentage = 0.0;
  CGFloat endPercentage = 0.0;
  switch (position) {
    case LEFT:
      startPercentage = 0.0;
      endPercentage = 0.5;
      break;
    case RIGHT:
      startPercentage = 0.5;
      endPercentage = 1.0;
      break;
    case CENTER:
      startPercentage = 0.0;
      endPercentage = 1.0;
      break;
    case ON_FIRST_LINE:
      NOTREACHED();
  }
  DCHECK(startPercentage >= 0.0 && startPercentage <= 1.0);
  DCHECK(endPercentage >= 0.0 && endPercentage <= 1.0);
  DCHECK(startPercentage < endPercentage);
  // In Material the button is not stretched to fit the available space. It is
  // placed centrally in the allotted space.
  CGFloat minX = screenWidth * startPercentage;
  CGFloat maxX = screenWidth * endPercentage;
  CGFloat midpoint = (minX + maxX) / 2;
  CGFloat minWidth =
      std::min([self narrowestWidthOfButton:button], maxX - minX);
  CGFloat left = midpoint - minWidth / 2;
  CGRect frame = CGRectMake(left, y, minWidth, metrics_->button_height);
  frame = AlignRectOriginAndSizeToPixels(frame);
  [button setFrame:frame];
}

- (CGFloat)layoutWideButtonAlignRight:(UIButton*)button
                            rightEdge:(CGFloat)rightEdge
                                    y:(CGFloat)y {
  CGFloat width = [self narrowestWidthOfButton:button];
  CGFloat leftEdge = rightEdge - width;
  CGRect frame = CGRectMake(leftEdge, y, width, metrics_->button_height);
  frame = AlignRectOriginAndSizeToPixels(frame);
  [button setFrame:frame];
  return leftEdge;
}

- (CGFloat)heightThatFitsButtonsUnderOtherWidgets:(CGFloat)heightOfFirstLine
                                           layout:(BOOL)layout {
  if (button1_ && button2_) {
    CGFloat halfWidthOfScreen = [self frame].size.width / 2.0;
    if ([self narrowestWidthOfButton:button1_] <= halfWidthOfScreen &&
        [self narrowestWidthOfButton:button2_] <= halfWidthOfScreen) {
      // Each button can fit in half the screen's width.
      if (layout) {
        // When there are two buttons on one line, they are positioned aligned
        // right in the available space, spaced apart by
        // metrics_->button_spacing.
        CGFloat leftOfRightmostButton =
            [self layoutWideButtonAlignRight:button1_
                                   rightEdge:CGRectGetWidth(self.bounds) -
                                             metrics_->button_margin -
                                             self.safeAreaInsets.right
                                           y:heightOfFirstLine];
        [self layoutWideButtonAlignRight:button2_
                               rightEdge:leftOfRightmostButton -
                                         metrics_->button_spacing
                                       y:heightOfFirstLine];
      }
      return metrics_->button_height;
    } else {
      // At least one of the two buttons is larger than half the screen's width,
      // so |button2_| is placed underneath |button1_|.
      if (layout) {
        [self layoutWideButton:button1_ y:heightOfFirstLine position:CENTER];
        [self layoutWideButton:button2_
                             y:heightOfFirstLine + metrics_->button_height
                      position:CENTER];
      }
      return 2 * metrics_->button_height;
    }
  }
  // There is at most 1 button to layout.
  UIButton* button = button1_ ? button1_ : button2_;
  if (button) {
    if (layout) {
      // Where is there is just one button it is positioned aligned right in the
      // available space.
      [self layoutWideButtonAlignRight:button
                             rightEdge:CGRectGetWidth(self.bounds) -
                                       metrics_->button_margin -
                                       self.safeAreaInsets.right
                                     y:heightOfFirstLine];
    }
    return metrics_->button_height;
  }
  return 0;
}

- (CGFloat)computeRequiredHeightAndLayoutSubviews:(BOOL)layout {
  CGFloat requiredHeight = 0;
  CGFloat widthOfLabel = [self widthOfLabelOnASingleLine] +
                         [self widthToSeparateRightAndLeftWidgets];
  CGFloat widthOfButtons = [self widthOfButtonsOnFirstLine];
  CGFloat preferredWidthOfSwitch = [self preferredWidthOfSwitch];
  CGFloat widthOfScreen = [self frame].size.width;
  CGFloat rightMarginOnFirstLine = [self rightMarginOnFirstLine];
  CGFloat spaceAvailableOnFirstLine =
      [self horizontalSpaceAvailableOnFirstLine];
  CGFloat widthOfButtonAndSwitch = widthOfButtons +
                                   [self widthToSeparateSwitchAndButtons] +
                                   preferredWidthOfSwitch;
  // Tests if the label, switch, and buttons can fit on a single line.
  if (widthOfLabel + widthOfButtonAndSwitch < spaceAvailableOnFirstLine) {
    // The label, switch, and buttons can fit on a single line.
    requiredHeight = metrics_->minimum_infobar_height;
    if (layout) {
      // Lays out the close button.
      CGRect buttonFrame = [self frameOfCloseButton:YES];
      [closeButton_ setFrame:buttonFrame];
      // Lays out the label.
      CGFloat labelHeight = [self heightRequiredForLabelWithWidth:widthOfLabel];
      CGRect frame =
          CGRectMake([self leftMarginOnFirstLine],
                     (metrics_->minimum_infobar_height - labelHeight) / 2,
                     [self widthOfLabelOnASingleLine], labelHeight);
      frame = AlignRectOriginAndSizeToPixels(frame);
      [label_ setFrame:frame];
      // Layouts the buttons.
      CGFloat buttonMargin =
          rightMarginOnFirstLine + metrics_->extra_button_margin_on_single_line;
      if (button1_) {
        CGFloat width = [self narrowestWidthOfButton:button1_];
        CGFloat offset = width;
        frame = CGRectMake(
            widthOfScreen - buttonMargin - offset,
            (metrics_->minimum_infobar_height - metrics_->button_height) / 2,
            width, metrics_->button_height);
        frame = AlignRectOriginAndSizeToPixels(frame);
        [button1_ setFrame:frame];
      }
      if (button2_) {
        CGFloat width = [self narrowestWidthOfButton:button2_];
        CGFloat offset = widthOfButtons;
        frame = CGRectMake(
            widthOfScreen - buttonMargin - offset,
            (metrics_->minimum_infobar_height - metrics_->button_height) / 2,
            width, frame.size.height = metrics_->button_height);
        frame = AlignRectOriginAndSizeToPixels(frame);
        [button2_ setFrame:frame];
      }
      // Lays out the switch view to the left of the buttons.
      if (InfobarFooterView_) {
        frame = CGRectMake(
            widthOfScreen - buttonMargin - widthOfButtonAndSwitch,
            (metrics_->minimum_infobar_height -
             [InfobarFooterView_ frame].size.height) /
                2.0,
            preferredWidthOfSwitch, [InfobarFooterView_ frame].size.height);
        frame = AlignRectOriginAndSizeToPixels(frame);
        [InfobarFooterView_ setFrame:frame];
      }
    }
  } else {
    // The widgets (label, switch, buttons) can't fit on a single line. Attempts
    // to lay out the label and switch on the first line, and the buttons
    // underneath.
    CGFloat heightOfLabelAndSwitch = 0;

    if (layout) {
      // Lays out the close button.
      CGRect buttonFrame = [self frameOfCloseButton:NO];
      [closeButton_ setFrame:buttonFrame];
    }
    if (widthOfLabel + preferredWidthOfSwitch < spaceAvailableOnFirstLine) {
      // The label and switch can fit on the first line.
      heightOfLabelAndSwitch = metrics_->minimum_infobar_height;
      if (layout) {
        CGFloat labelHeight =
            [self heightRequiredForLabelWithWidth:widthOfLabel];
        CGRect labelFrame =
            CGRectMake([self leftMarginOnFirstLine],
                       (heightOfLabelAndSwitch - labelHeight) / 2,
                       [self widthOfLabelOnASingleLine], labelHeight);
        labelFrame = AlignRectOriginAndSizeToPixels(labelFrame);
        [label_ setFrame:labelFrame];
        if (InfobarFooterView_) {
          CGRect switchRect = CGRectMake(
              widthOfScreen - rightMarginOnFirstLine - preferredWidthOfSwitch,
              (heightOfLabelAndSwitch -
               [InfobarFooterView_ frame].size.height) /
                  2,
              preferredWidthOfSwitch, [InfobarFooterView_ frame].size.height);
          switchRect = AlignRectOriginAndSizeToPixels(switchRect);
          [InfobarFooterView_ setFrame:switchRect];
        }
      }
    } else {
      // The label and switch can't fit on the first line, so lay them out on
      // different lines.
      // Computes the height of the label, and optionally lays it out.
      CGFloat labelMarginBottom = metrics_->label_margin_bottom;
      if (button1_ || button2_) {
        // Material features more padding between the label and the button than
        // the label and the bottom of the dialog when there is no button.
        labelMarginBottom += metrics_->extra_margin_between_label_and_button;
      }
      CGFloat heightOfLabelWithPadding =
          [self heightRequiredForLabelWithWidth:spaceAvailableOnFirstLine] +
          metrics_->label_margin_top + labelMarginBottom;
      if (layout) {
        CGRect labelFrame =
            CGRectMake([self leftMarginOnFirstLine], metrics_->label_margin_top,
                       spaceAvailableOnFirstLine,
                       heightOfLabelWithPadding - metrics_->label_margin_top -
                           labelMarginBottom);
        labelFrame = AlignRectOriginAndSizeToPixels(labelFrame);
        [label_ setFrame:labelFrame];
      }
      // Computes the height of the switch view (if any), and optionally lays it
      // out.
      CGFloat heightOfSwitchWithPadding = 0;
      if (InfobarFooterView_ != nil) {
        // The switch view is aligned with the first line's label, hence the
        // call to |leftMarginOnFirstLine|.
        CGFloat widthAvailableForSwitchView = [self frame].size.width -
                                              [self leftMarginOnFirstLine] -
                                              metrics_->right_margin;
        CGFloat heightOfSwitch = [InfobarFooterView_
            heightRequiredForFooterWithWidth:widthAvailableForSwitchView
                                      layout:layout];
        // If there are buttons underneath the switch, add padding.
        if (button1_ || button2_) {
          heightOfSwitchWithPadding =
              heightOfSwitch + metrics_->space_between_widgets +
              metrics_->extra_margin_between_label_and_button;
        } else {
          heightOfSwitchWithPadding = heightOfSwitch;
        }
        if (layout) {
          CGRect switchRect =
              CGRectMake([self leftMarginOnFirstLine], heightOfLabelWithPadding,
                         widthAvailableForSwitchView, heightOfSwitch);
          switchRect = AlignRectOriginAndSizeToPixels(switchRect);
          [InfobarFooterView_ setFrame:switchRect];
        }
      }
      heightOfLabelAndSwitch =
          std::max(heightOfLabelWithPadding + heightOfSwitchWithPadding,
                   metrics_->minimum_infobar_height);
    }
    // Lays out the button(s) under the label and switch.
    CGFloat heightOfButtons =
        [self heightThatFitsButtonsUnderOtherWidgets:heightOfLabelAndSwitch
                                              layout:layout];
    requiredHeight = heightOfLabelAndSwitch;
    if (heightOfButtons > 0)
      requiredHeight += heightOfButtons + metrics_->button_margin;
  }
  // Take into account the bottom safe area.
  // The top safe area is ignored because at rest (i.e. not during animations)
  // the infobar is aligned to the bottom of the screen, and thus should not
  // have its top intersect with any safe area.
  CGFloat bottomSafeAreaInset = self.safeAreaInsets.bottom;
  requiredHeight += bottomSafeAreaInset;

  UILayoutGuide* guide =
      [NamedGuide guideWithName:kSecondaryToolbarGuide view:self];
  UILayoutGuide* guideNoFullscreen =
      [NamedGuide guideWithName:kSecondaryToolbarNoFullscreenGuide view:self];
  if (guide && guideNoFullscreen) {
    CGFloat toolbarHeightCurrent = guide.layoutFrame.size.height;
    CGFloat toolbarHeightMax = guideNoFullscreen.layoutFrame.size.height;
    if (toolbarHeightMax > 0) {
      CGFloat fullscreenProgress = toolbarHeightCurrent / toolbarHeightMax;
      CGFloat toolbarHeightInSafeArea = toolbarHeightMax - bottomSafeAreaInset;
      requiredHeight += fullscreenProgress * toolbarHeightInSafeArea;
    }
  }

  return requiredHeight;
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGFloat requiredHeight = [self computeRequiredHeightAndLayoutSubviews:NO];
  return CGSizeMake([self frame].size.width, requiredHeight);
}

- (void)layoutSubviews {
  // Add the separator if it's not already added.
  if (self.separator.superview != self) {
    [self addSubview:self.separator];
    CGFloat separatorHeight =
        ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);
    [NSLayoutConstraint activateConstraints:@[
      [self.separator.heightAnchor constraintEqualToConstant:separatorHeight],
      [self.leadingAnchor constraintEqualToAnchor:self.separator.leadingAnchor],
      [self.trailingAnchor
          constraintEqualToAnchor:self.separator.trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:self.separator.bottomAnchor],
    ]];
  }

  // Lays out the position of the icon.
  [imageView_ setFrame:[self frameOfIcon]];
  self.visibleHeight = [self computeRequiredHeightAndLayoutSubviews:YES];
  [self resetBackground];

  // Asks the BidiContainerView to reposition of all the subviews.
  for (UIView* view in [self subviews])
    [self setSubviewNeedsAdjustmentForRTL:view];
  [super layoutSubviews];
}

- (void)resetBackground {
  self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  CGFloat shadowY = 0;
  shadowY = -[shadow_ image].size.height;  // Shadow above the infobar.
  [shadow_ setFrame:CGRectMake(0, shadowY, self.bounds.size.width,
                               [shadow_ image].size.height)];
  [shadow_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
}

- (void)addCloseButtonWithTag:(NSInteger)tag
                       target:(id)target
                       action:(SEL)action {
  DCHECK(!closeButton_);
  UIImage* image = InfoBarCloseImage();
  closeButton_ = [UIButton buttonWithType:UIButtonTypeSystem];
  [closeButton_ setExclusiveTouch:YES];
  [closeButton_ setImage:image forState:UIControlStateNormal];
  [closeButton_ addTarget:target
                   action:action
         forControlEvents:UIControlEventTouchUpInside];
  [closeButton_ setTag:tag];
  [closeButton_ setAccessibilityLabel:l10n_util::GetNSString(IDS_CLOSE)];
  closeButton_.tintColor = [UIColor colorNamed:kToolbarButtonColor];
  [self addSubview:closeButton_];
}

- (void)addSwitchWithLabel:(NSString*)label
                      isOn:(BOOL)isOn
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action {
  SwitchView* switchView = [[SwitchView alloc] initWithText:label isOn:isOn];
  [switchView setTag:tag target:target action:action];
  InfobarFooterView_ = switchView;
  [self addSubview:InfobarFooterView_];
}

- (void)addFooterLabel:(NSString*)label {
  InfobarFooterView_ = [[InfobarFooterView alloc] initWithText:label];
  [self addSubview:InfobarFooterView_];
}

- (void)addLeftIcon:(UIImage*)image {
  if (imageView_) {
    [imageView_ removeFromSuperview];
  }
  UIImage* templateImage =
      [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  imageView_ = [[UIImageView alloc] initWithImage:templateImage];
  [self addSubview:imageView_];
}

- (NSString*)stripMarkersFromString:(NSString*)string {
  linkRanges_.clear();
  for (;;) {
    // Find the opening marker, followed by the tag between parentheses.
    NSRange startingRange =
        [string rangeOfString:[[ConfirmInfoBarView openingMarkerForLink]
                                  stringByAppendingString:@"("]];
    if (!startingRange.length)
      return [string copy];
    // Read the tag.
    NSUInteger beginTag = NSMaxRange(startingRange);
    NSRange closingParenthesis = [string
        rangeOfString:@")"
              options:NSLiteralSearch
                range:NSMakeRange(beginTag, [string length] - beginTag)];
    if (closingParenthesis.location == NSNotFound)
      return [string copy];
    NSInteger tag = [[string
        substringWithRange:NSMakeRange(beginTag, closingParenthesis.location -
                                                     beginTag)] integerValue];
    // If the parsing fails, |tag| is 0. Negative values are not allowed.
    if (tag <= 0)
      return [string copy];
    // Find the closing marker.
    startingRange.length =
        closingParenthesis.location - startingRange.location + 1;
    NSRange endingRange =
        [string rangeOfString:[ConfirmInfoBarView closingMarkerForLink]];
    DCHECK(endingRange.length);
    // Compute range of link in stripped string and add it to the array.
    NSRange rangeOfLinkInStrippedString =
        NSMakeRange(startingRange.location,
                    endingRange.location - NSMaxRange(startingRange));
    linkRanges_.push_back(std::make_pair(tag, rangeOfLinkInStrippedString));
    // Creates a new string without the markers.
    NSString* beforeLink = [string substringToIndex:startingRange.location];
    NSRange rangeOfLink =
        NSMakeRange(NSMaxRange(startingRange),
                    endingRange.location - NSMaxRange(startingRange));
    NSString* link = [string substringWithRange:rangeOfLink];
    NSString* afterLink = [string substringFromIndex:NSMaxRange(endingRange)];
    string = [NSString stringWithFormat:@"%@%@%@", beforeLink, link, afterLink];
  }
}

- (void)addLabel:(NSString*)label {
  [self addLabel:label action:nil];
}

- (void)addLabel:(NSString*)text action:(void (^)(NSUInteger))action {
  markedLabel_ = [text copy];
  if (action)
    text = [self stripMarkersFromString:text];
  if ([label_ superview]) {
    [label_ removeFromSuperview];
  }

  label_ = [[UILabel alloc] initWithFrame:CGRectZero];
  label_.textColor = [UIColor colorNamed:kTextPrimaryColor];

  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
  paragraphStyle.lineSpacing = metrics_->label_line_spacing;
  NSDictionary* attributes = @{
    NSParagraphStyleAttributeName : paragraphStyle,
    NSFontAttributeName : InfoBarLabelFont(),
  };
  [label_ setNumberOfLines:0];

  [label_
      setAttributedText:[[NSAttributedString alloc] initWithString:text
                                                        attributes:attributes]];

  [self addSubview:label_];

  if (linkRanges_.empty())
    return;

  labelLinkController_ = [[LabelLinkController alloc]
      initWithLabel:label_
             action:^(const GURL& gurl) {
               if (action) {
                 NSUInteger actionTag = [base::SysUTF8ToNSString(
                     gurl.ExtractFileName()) integerValue];
                 action(actionTag);
               }
             }];

  [labelLinkController_ setLinkUnderlineStyle:NSUnderlineStyleSingle];
  [labelLinkController_ setLinkColor:[UIColor colorNamed:kTextPrimaryColor]];

  std::vector<std::pair<NSUInteger, NSRange>>::const_iterator it;
  for (it = linkRanges_.begin(); it != linkRanges_.end(); ++it) {
    // The last part of the URL contains the tag, so it can be retrieved in the
    // callback. This tag is generally a command ID.
    std::string url = std::string(kChromeInfobarURL) +
                      std::string(std::to_string((int)it->first));
    [labelLinkController_ addLinkWithRange:it->second url:GURL(url)];
  }
}

- (void)addButton1:(NSString*)title1
              tag1:(NSInteger)tag1
           button2:(NSString*)title2
              tag2:(NSInteger)tag2
            target:(id)target
            action:(SEL)action {
  button1_ = [self infoBarButton:title1
                 backgroundColor:[UIColor colorNamed:kBlueColor]
                customTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                             tag:tag1
                          target:target
                          action:action];
  [button1_
      setAccessibilityIdentifier:kConfirmInfobarButton1AccessibilityIdentifier];
  [self addSubview:button1_];

  button2_ = [self infoBarButton:title2
                 backgroundColor:nil
                customTitleColor:[UIColor colorNamed:kBlueColor]
                             tag:tag2
                          target:target
                          action:action];
  [button2_
      setAccessibilityIdentifier:kConfirmInfobarButton2AccessibilityIdentifier];
  [self addSubview:button2_];
}

- (void)addButton:(NSString*)title
              tag:(NSInteger)tag
           target:(id)target
           action:(SEL)action {
  if (![title length])
    return;
  button1_ = [self infoBarButton:title
                 backgroundColor:[UIColor colorNamed:kBlueColor]
                customTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
                             tag:tag
                          target:target
                          action:action];
  [self addSubview:button1_];
}

// Initializes and returns a button for the infobar, with the specified
// |message| and colors.
- (UIButton*)infoBarButton:(NSString*)message
           backgroundColor:(UIColor*)backgroundColor
          customTitleColor:(UIColor*)customTitleColor
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action {
  MDCFlatButton* button = [[MDCFlatButton alloc] init];
  button.uppercaseTitle = NO;
  button.layer.cornerRadius = kButtonCornerRadius;
  [button setTitleFont:InfoBarButtonLabelFont() forState:UIControlStateNormal];
  button.inkColor = [UIColor colorNamed:kMDCInkColor];
  [button setBackgroundColor:backgroundColor forState:UIControlStateNormal];
  [button setBackgroundColor:[UIColor colorNamed:kDisabledTintColor]
                    forState:UIControlStateDisabled];
  if (backgroundColor)
    button.hasOpaqueBackground = YES;
  if (customTitleColor) {
    button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
    [button setTitleColor:customTitleColor forState:UIControlStateNormal];
  }
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.minimumScaleFactor = 0.6f;
  [button setTitle:message forState:UIControlStateNormal];
  [button setTag:tag];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  // Without the call to layoutIfNeeded, |button| returns an incorrect
  // titleLabel the first time it is accessed in |narrowestWidthOfButton|.
  [button layoutIfNeeded];
  return button;
}

- (CGRect)frameOfCloseButton:(BOOL)singleLineMode {
  DCHECK(closeButton_);
  // Add padding to increase the touchable area.
  CGSize closeButtonSize = [closeButton_ imageView].image.size;
  closeButtonSize.width += metrics_->close_button_inner_padding * 2;
  closeButtonSize.height += metrics_->close_button_inner_padding * 2;
  CGFloat x = CGRectGetMaxX(self.frame) - closeButtonSize.width -
              self.safeAreaInsets.right;
  // Aligns the close button at the top (height includes touch padding).
  CGFloat y = 0;
  if (singleLineMode) {
    // On single-line mode the button is centered vertically.
    y = ui::AlignValueToUpperPixel(
        (metrics_->minimum_infobar_height - closeButtonSize.height) * 0.5);
  }
  return CGRectMake(x, y, closeButtonSize.width, closeButtonSize.height);
}

- (CGRect)frameOfIcon {
  CGSize iconSize = [imageView_ image].size;
  CGFloat y = metrics_->buttons_margin_top;
  CGFloat x = metrics_->close_button_margin_left + self.safeAreaInsets.left;
  return CGRectMake(AlignValueToPixel(x), AlignValueToPixel(y), iconSize.width,
                    iconSize.height);
}

+ (NSString*)openingMarkerForLink {
  return @"$LINK_START";
}

+ (NSString*)closingMarkerForLink {
  return @"$LINK_END";
}

+ (NSString*)stringAsLink:(NSString*)string tag:(NSUInteger)tag {
  DCHECK_NE(0u, tag);
  return [NSString stringWithFormat:@"%@(%" PRIuNS ")%@%@",
                                    [ConfirmInfoBarView openingMarkerForLink],
                                    tag, string,
                                    [ConfirmInfoBarView closingMarkerForLink]];
}

#pragma mark - Testing

- (CGFloat)minimumInfobarHeight {
  return metrics_->minimum_infobar_height;
}

- (CGFloat)buttonsHeight {
  return metrics_->button_height;
}

- (CGFloat)buttonMargin {
  return metrics_->button_margin;
}

- (const std::vector<std::pair<NSUInteger, NSRange>>&)linkRanges {
  return linkRanges_;
}

@end
