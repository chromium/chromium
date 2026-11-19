// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/browser/omnibox/ui/text_field_view_containing.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FakeOmniboxHeightDelegate
    : NSObject <TextFieldViewContainingHeightDelegate>
@property(nonatomic, assign) CGFloat lastHeight;
@end

@implementation FakeOmniboxHeightDelegate

- (void)textFieldViewContaining:(UIView<TextFieldViewContaining>*)sender
                didChangeHeight:(CGFloat)height {
  self.lastHeight = height;
}

@end

namespace {

class OmniboxContainerViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    window_ = [[UIWindow alloc] initWithFrame:CGRectMake(0, 0, 400, 400)];
    host_view_ = [[UIView alloc] initWithFrame:window_.bounds];
    [window_ addSubview:host_view_];
    container_view_ = [[OmniboxContainerView alloc]
              initWithFrame:CGRectZero
                  textColor:UIColor.blackColor
              textInputTint:UIColor.blackColor
                   iconTint:UIColor.blackColor
        presentationContext:OmniboxPresentationContext::kComposebox];
    container_view_.translatesAutoresizingMaskIntoConstraints = NO;
    container_view_.leadingImageHidden = YES;
    [container_view_ setClearButtonHidden:YES];
    height_delegate_ = [[FakeOmniboxHeightDelegate alloc] init];
    container_view_.heightDelegate = height_delegate_;
    [host_view_ addSubview:container_view_];
    width_constraint_ =
        [container_view_.widthAnchor constraintEqualToConstant:262.0];
    [NSLayoutConstraint activateConstraints:@[
      [container_view_.leadingAnchor
          constraintEqualToAnchor:host_view_.leadingAnchor],
      [container_view_.topAnchor constraintEqualToAnchor:host_view_.topAnchor],
      width_constraint_,
    ]];
  }

  void TearDown() override {
    container_view_.heightDelegate = nil;
    [container_view_ removeFromSuperview];
    [host_view_ removeFromSuperview];
    window_.hidden = YES;
    width_constraint_ = nil;
    height_delegate_ = nil;
    container_view_ = nil;
    host_view_ = nil;
    window_ = nil;
    PlatformTest::TearDown();
  }

  // Applies `horizontal_size_class` with Regular vertical size class and a
  // Large content size category, updates the container width to `width`, and
  // performs an initial layout pass.
  void ApplyTraitSizeClass(UIUserInterfaceSizeClass horizontal_size_class,
                           CGFloat width) {
    host_view_.traitOverrides.horizontalSizeClass = horizontal_size_class;
    host_view_.traitOverrides.verticalSizeClass =
        UIUserInterfaceSizeClassRegular;
    host_view_.traitOverrides.preferredContentSizeCategory =
        UIContentSizeCategoryLarge;
    width_constraint_.constant = width;
    [GetTextView() updateTraitsIfNeeded];
    [host_view_ setNeedsLayout];
    [container_view_ setNeedsLayout];
    LayoutContainer();
  }

  // Flushes pending layout passes on the window, host, container, and text
  // views.
  void LayoutContainer() {
    [window_ layoutIfNeeded];
    [host_view_ layoutIfNeeded];
    [container_view_ layoutIfNeeded];
    UITextView* text_view = GetTextView();
    [text_view setNeedsLayout];
    [text_view layoutIfNeeded];
  }

  // Returns the container's underlying `UITextView`.
  UITextView* GetTextView() {
    return base::apple::ObjCCastStrict<UITextView>(
        container_view_.textFieldView);
  }

  base::test::TaskEnvironment task_environment_;
  UIWindow* window_;
  UIView* host_view_;
  OmniboxContainerView* container_view_;
  FakeOmniboxHeightDelegate* height_delegate_;
  NSLayoutConstraint* width_constraint_;
};

// Tests that a multiline query ending in a long dotted word expands to all laid
// out lines and does not clip the trailing line to an empty rect.
TEST_F(OmniboxContainerViewTest,
       ExpandsForMultilineInputWithLongDottedWordUnderCompactWidth) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  ASSERT_EQ(text_view.bounds.size.width, 262.0);
  NSString* dotted_query = @"a b c d e f g h i j k l "
                           @"t.p.q.t.s.t.u.v.w.x.y.z.aa.bb.ce.dd.ee.ff.gg.hh."
                           @"ii.jj.kk.ll.mm.nn.oo.pp.qq.rr";
  ASSERT_EQ(dotted_query.length, 101u);

  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:dotted_query]
      userTextLength:dotted_query.length];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 4u);
  EXPECT_EQ(height_delegate_.lastHeight, 96.0);
  EXPECT_EQ(text_view.textContainer.maximumNumberOfLines, 0u);
  [text_view.layoutManager
      ensureLayoutForTextContainer:text_view.textContainer];
  NSUInteger last_glyph_index =
      [text_view.layoutManager glyphIndexForCharacterAtIndex:100u];
  CGRect last_line_rect =
      [text_view.layoutManager lineFragmentRectForGlyphAtIndex:last_glyph_index
                                                effectiveRange:nullptr];
  EXPECT_FALSE(CGRectIsEmpty(last_line_rect));
}

// Tests that a long dotted word under regular width does not over-expand past
// the actual number of lines laid out by the text view.
TEST_F(OmniboxContainerViewTest,
       DoesNotOverExpandForLongDottedWordUnderRegularWidth) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassRegular, 229.0);
  UITextView* text_view = GetTextView();
  ASSERT_EQ(text_view.bounds.size.width, 229.0);
  NSString* dotted_query =
      @"djsndbf foo.bar.baz.qux.quux.corge.grault.garply.waldo.fred";

  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:dotted_query]
      userTextLength:dotted_query.length];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 3u);
}

// Tests that inline autocomplete text does not increase `numberOfLines` and
// remains capped to the user text's line count.
TEST_F(OmniboxContainerViewTest,
       CapsMaximumNumberOfLinesWhenInlineAutocompleteIsPresent) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  NSString* full_text =
      @"hello world with long inline autocomplete suggestion that overflows";

  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:full_text]
      userTextLength:11];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 1u);
  EXPECT_EQ(text_view.textContainer.maximumNumberOfLines, 1u);
}

// Tests that entering pre-edit state collapses a multiline URL to a single
// clipped line.
TEST_F(OmniboxContainerViewTest, CollapsesToSingleLineInPreEditState) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  NSString* long_url = @"https://example.com/very/long/path/that/would/wrap/"
                       @"across/multiple/lines";
  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:long_url]
      userTextLength:long_url.length];
  LayoutContainer();
  ASSERT_GT(container_view_.numberOfLines, 1u);

  [container_view_.textInput enterPreEditState];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 1u);
  EXPECT_EQ(text_view.textContainer.maximumNumberOfLines, 1u);
}

// Tests that a trailing newline expands the omnibox to include the empty line.
TEST_F(OmniboxContainerViewTest, ExpandsForTrailingNewline) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  NSString* text_with_newline = @"First line\n";

  [container_view_.textInput setText:[[NSAttributedString alloc]
                                         initWithString:text_with_newline]
                      userTextLength:text_with_newline.length];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 2u);
}

// Tests that `scrollEnabled` turns on when user text exceeds the 5-line
// Composebox height cap.
TEST_F(OmniboxContainerViewTest, EnablesScrollingWhenExceedingMaxLines) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  NSString* five_lines = @"Line 1\nLine 2\nLine 3\nLine 4\nLine 5";
  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:five_lines]
      userTextLength:five_lines.length];
  LayoutContainer();
  ASSERT_EQ(container_view_.numberOfLines, 5u);
  ASSERT_FALSE(text_view.scrollEnabled);

  NSString* six_lines = @"Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6";
  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:six_lines]
      userTextLength:six_lines.length];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 6u);
  EXPECT_TRUE(text_view.scrollEnabled);
}

// Tests that trailing additional text on a multiline query does not increase
// `numberOfLines` and caps `maximumNumberOfLines` to the user text's lines.
TEST_F(OmniboxContainerViewTest,
       CapsMaximumNumberOfLinesWhenAdditionalTextIsPresent) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  NSString* two_line_user_text = @"First line\nSecond line";
  [container_view_.textInput setText:[[NSAttributedString alloc]
                                         initWithString:two_line_user_text]
                      userTextLength:two_line_user_text.length];

  [container_view_.textInput
      setAdditionalText:
          @" - long additional text suffix that overflows the second line"];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 2u);
  EXPECT_EQ(text_view.textContainer.maximumNumberOfLines, 2u);
}

// Tests that exiting pre-edit state restores the multiline count and uncaps
// `maximumNumberOfLines`.
TEST_F(OmniboxContainerViewTest,
       UncapsMaximumNumberOfLinesWhenExitingPreEditState) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  NSString* long_url = @"https://example.com/very/long/path/that/would/wrap/"
                       @"across/multiple/lines";
  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:long_url]
      userTextLength:long_url.length];
  [container_view_.textInput enterPreEditState];
  LayoutContainer();
  ASSERT_EQ(container_view_.numberOfLines, 1u);
  ASSERT_EQ(text_view.textContainer.maximumNumberOfLines, 1u);

  [container_view_.textInput exitPreEditState];
  LayoutContainer();

  EXPECT_GT(container_view_.numberOfLines, 1u);
  EXPECT_EQ(text_view.textContainer.maximumNumberOfLines, 0u);
}

// Tests that `scrollEnabled` turns back off when user text shrinks from 6
// lines back to the 5-line Composebox height cap.
TEST_F(OmniboxContainerViewTest, DisablesScrollingWhenShrinkingBackToMaxLines) {
  ApplyTraitSizeClass(UIUserInterfaceSizeClassCompact, 262.0);
  UITextView* text_view = GetTextView();
  NSString* six_lines = @"Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6";
  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:six_lines]
      userTextLength:six_lines.length];
  LayoutContainer();
  ASSERT_EQ(container_view_.numberOfLines, 6u);
  ASSERT_TRUE(text_view.scrollEnabled);

  NSString* five_lines = @"Line 1\nLine 2\nLine 3\nLine 4\nLine 5";
  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:five_lines]
      userTextLength:five_lines.length];
  LayoutContainer();

  EXPECT_EQ(container_view_.numberOfLines, 5u);
  EXPECT_FALSE(text_view.scrollEnabled);
}

// Tests that `numberOfLines` returns 1 in single-line `UITextField`
// presentation contexts such as `kLocationBar`.
TEST_F(OmniboxContainerViewTest,
       ReturnsSingleLineForLocationBarPresentationContext) {
  OmniboxContainerView* location_bar_container = [[OmniboxContainerView alloc]
            initWithFrame:CGRectMake(0, 0, 262.0, 44.0)
                textColor:UIColor.blackColor
            textInputTint:UIColor.blackColor
                 iconTint:UIColor.blackColor
      presentationContext:OmniboxPresentationContext::kLocationBar];
  NSString* multiline_text = @"Line 1\nLine 2\nLine 3";

  [location_bar_container.textInput
             setText:[[NSAttributedString alloc] initWithString:multiline_text]
      userTextLength:multiline_text.length];

  EXPECT_EQ(location_bar_container.numberOfLines, 1u);
}

// Tests that `numberOfLines` defaults to 1 when non-empty text is set before
// initial layout while the text view's drawing width is non-positive.
TEST_F(OmniboxContainerViewTest,
       DefaultsToSingleLineBeforeInitialLayoutWhenDrawingWidthIsZero) {
  UITextView* text_view = GetTextView();
  ASSERT_LE(text_view.bounds.size.width,
            text_view.textContainer.lineFragmentPadding * 2.0);
  NSString* multiline_text = @"Line 1\nLine 2\nLine 3";

  [container_view_.textInput
             setText:[[NSAttributedString alloc] initWithString:multiline_text]
      userTextLength:multiline_text.length];

  EXPECT_EQ(container_view_.numberOfLines, 1u);
}

}  // namespace
