// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label.h"
#import "ios/chrome/browser/shared/ui/elements/fade_truncating_label+Testing.h"

#import <vector>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

/// Width used for `kShortText`, `kTwoLineText` and `kThreeLineText` to be true.
const NSInteger kLimitedWidth = 200;
/// Text that can be drawn on one line using `kLimitedWidth`.
const NSString* kShortText = @"short text";
/// Text that can be drawn on two lines using `kLimitedWidth`.
const NSString* kTwoLinesText = @"text that should wrap on two lines";
/// Text that can be drawn on three lines using `kLimitedWidth`.
const NSString* kThreeLinesText =
    @"Long text that should take three lines when displayed";

/// Returns the number of line needed to draw `label` in `bounds`.
NSInteger NumberOfDrawnLine(UILabel* label, CGRect bounds) {
  CGRect bounding_rect = [label textRectForBounds:bounds
                           limitedToNumberOfLines:label.numberOfLines];
  NSInteger number_of_drawn_line =
      bounding_rect.size.height / label.font.lineHeight;
  return number_of_drawn_line;
}

/// Struct used to store parameters of `drawAttributedString`.
struct DrawAttributedStringParams {
  NSAttributedString* attributed_string;
  CGRect rect;
  BOOL apply_gradient;
  CGFloat alignment_offset;
};

class FadeTruncatingLabelTest : public PlatformTest {
 public:
  FadeTruncatingLabelTest() : drawAttributedString_params_() {}

 protected:
  void SetUp() override {
    short_text_ = [[FadeTruncatingLabel alloc] init];
    short_text_.text = [kShortText copy];
    two_lines_text_ = [[FadeTruncatingLabel alloc] init];
    two_lines_text_.text = [kTwoLinesText copy];
    three_lines_text_ = [[FadeTruncatingLabel alloc] init];
    three_lines_text_.text = [kThreeLinesText copy];
    labels_ = @[ short_text_, two_lines_text_, three_lines_text_ ];

    for (FadeTruncatingLabel* label in labels_) {
      StubDrawAttributedStringInRect(label);
    }

    drawAttributedString_params_.clear();
  }

  /// Stubs calls to `drawAttributedString` and store parameters in
  /// `drawAttributedString_params_`.
  void StubDrawAttributedStringInRect(FadeTruncatingLabel* label) {
    id partial_mock = OCMPartialMock(label);
    OCMStub([[partial_mock ignoringNonObjectArgs]
                drawAttributedString:[OCMArg any]
                              inRect:CGRectZero
                       applyGradient:NO
                     alignmentOffset:0.0])
        .andDo(^(NSInvocation* invocation) {
          __unsafe_unretained NSAttributedString* attributed_string = nil;
          CGRect rect = CGRectZero;
          BOOL apply_gradient = NO;
          CGFloat alignment_offset = 0.0;
          [invocation getArgument:&attributed_string atIndex:2];
          [invocation getArgument:&rect atIndex:3];
          [invocation getArgument:&apply_gradient atIndex:4];
          [invocation getArgument:&alignment_offset atIndex:5];
          drawAttributedString_params_.push_back(
              {attributed_string, rect, apply_gradient, alignment_offset});
        });
  }

  /// Checks that calls to `textRectForBounds` with `bounds` on `label` returns
  /// valid bounds.
  void ExpectValidBoundingRect(FadeTruncatingLabel* label, CGRect bounds) {
    CGRect bounding_rect = [label textRectForBounds:bounds
                             limitedToNumberOfLines:label.numberOfLines];

    EXPECT_TRUE(CGRectContainsRect(bounds, bounding_rect));

    // Compare `bounding_rect` with the bounding rect returned by `UILabel`.
    UILabel* ui_label = [[UILabel alloc] init];
    ui_label.lineBreakMode = NSLineBreakByWordWrapping;
    ui_label.text = label.text;
    ui_label.numberOfLines = label.numberOfLines;
    CGRect ui_label_bounding_rect =
        [ui_label textRectForBounds:bounds
             limitedToNumberOfLines:label.numberOfLines];
    EXPECT_NEAR(bounding_rect.size.height, ui_label_bounding_rect.size.height,
                1.0);
    // FadeTruncatingLabel fills the last line, so it might be wider than
    // UILabel.
    EXPECT_GE(bounding_rect.size.width, ui_label_bounding_rect.size.width);
  }

  /// Checks that only the last line `should_have_gradient`.
  void ExpectGradientOnLastLine(BOOL should_have_gradient) {
    if (drawAttributedString_params_.empty()) {
      return;
    }
    const size_t last_line = drawAttributedString_params_.size() - 1;
    for (size_t i = 0; i < last_line; i++) {
      EXPECT_FALSE(drawAttributedString_params_[i].apply_gradient);
    }
    EXPECT_EQ(!drawAttributedString_params_[last_line].apply_gradient,
              !should_have_gradient);
  }

  /// Checks that calls to `drawAttributedString` use valid rect when drawing in
  /// `bounds`.
  void ExpectValidDrawingRect(CGRect bounds) {
    if (drawAttributedString_params_.empty()) {
      return;
    }
    // When the available height is smaller than one line, it start to draw
    // outside of `bounds` to center the text in the available space.
    if (bounds.size.height < short_text_.font.lineHeight + FLT_EPSILON) {
      EXPECT_EQ(drawAttributedString_params_.size(), 1u);
      const DrawAttributedStringParams& param = drawAttributedString_params_[0];
      CGPoint start_x = CGPointMake(param.rect.origin.x + FLT_EPSILON,
                                    CGRectGetMidY(param.rect));
      CGPoint end_x =
          CGPointMake(param.rect.origin.x + param.rect.size.width - FLT_EPSILON,
                      CGRectGetMidY(param.rect));
      EXPECT_TRUE(CGRectContainsPoint(bounds, start_x));
      EXPECT_TRUE(CGRectContainsPoint(bounds, end_x));
    } else {
      for (const DrawAttributedStringParams& param :
           drawAttributedString_params_) {
        EXPECT_TRUE(CGRectContainsRect(bounds, param.rect));
      }
    }
  }

  std::vector<DrawAttributedStringParams> drawAttributedString_params_;
  NSArray<FadeTruncatingLabel*>* labels_;
  FadeTruncatingLabel* short_text_;
  FadeTruncatingLabel* two_lines_text_;
  FadeTruncatingLabel* three_lines_text_;
};

// Tests that this file's constants are valid.
TEST_F(FadeTruncatingLabelTest, ValidConstants) {
  CGRect bounds = CGRectMake(0, 0, kLimitedWidth, FLT_MAX);
  UILabel* label = [[UILabel alloc] init];
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.numberOfLines = 0;

  label.text = [kShortText copy];
  EXPECT_EQ(NumberOfDrawnLine(label, bounds), 1);
  label.text = [kTwoLinesText copy];
  EXPECT_EQ(NumberOfDrawnLine(label, bounds), 2);
  label.text = [kThreeLinesText copy];
  EXPECT_EQ(NumberOfDrawnLine(label, bounds), 3);
}

// Tests that FadeTruncatinglabel returns valid bounding rect when calling
// `textRectForBounds`
TEST_F(FadeTruncatingLabelTest, ValidBoundingRect) {
  CGFloat max_bound = 200;
  CGFloat bound_increment = 20;
  NSInteger max_number_of_lines = 4;

  for (CGFloat width = 0.0; width < max_bound; width += bound_increment) {
    for (CGFloat height = 0.0; height < max_bound; height += bound_increment) {
      for (NSInteger number_of_lines = 0; number_of_lines < max_number_of_lines;
           ++number_of_lines) {
        CGRect bounds = CGRectMake(0.0, 0.0, width, height);
        for (FadeTruncatingLabel* label in labels_) {
          label.numberOfLines = number_of_lines;
          ExpectValidBoundingRect(label, bounds);
        }
      }
    }
  }
}

// Tests that FadeTruncatingLabel draws with valid rect when calling
// `drawTextInRect`.
TEST_F(FadeTruncatingLabelTest, ValidDrawingRect) {
  CGFloat max_bound = 200;
  CGFloat bound_increment = 20;
  NSInteger max_number_of_lines = 4;

  for (CGFloat width = 1.0; width < max_bound; width += bound_increment) {
    for (CGFloat height = 1.0; height < max_bound; height += bound_increment) {
      for (NSInteger number_of_lines = 0; number_of_lines < max_number_of_lines;
           ++number_of_lines) {
        CGRect bounds = CGRectMake(0.0, 0.0, width, height);
        for (FadeTruncatingLabel* label in labels_) {
          label.numberOfLines = number_of_lines;
          drawAttributedString_params_.clear();
          [label drawTextInRect:bounds];
          ExpectValidDrawingRect(bounds);
        }
      }
    }
  }
}

// Tests that the gradient is only applied on the last line.
TEST_F(FadeTruncatingLabelTest, GradientOnLastLine) {
  CGRect bounds = CGRectMake(0, 0, kLimitedWidth, FLT_MAX);
  for (size_t label_index = 0; label_index < labels_.count; label_index++) {
    FadeTruncatingLabel* label = labels_[label_index];
    // Number of lines used by the label.
    const size_t label_number_of_line = label_index + 1;
    for (size_t number_of_lines = 1;
         number_of_lines <= label_number_of_line + 1; number_of_lines++) {
      drawAttributedString_params_.clear();
      label.numberOfLines = number_of_lines;
      CGRect text_rect = [label textRectForBounds:bounds
                           limitedToNumberOfLines:label.numberOfLines];
      [label drawTextInRect:text_rect];
      ExpectGradientOnLastLine(label_number_of_line > number_of_lines);
    }
  }
}

}  // namespace
