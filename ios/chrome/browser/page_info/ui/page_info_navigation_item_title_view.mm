// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/ui/page_info_navigation_item_title_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// The minimum scale factor of the title label.
constexpr CGFloat kTitleLabelMinimumScaleFactor = 0.8f;

// The vertical spacing between the prompt and the title in the stack view.
constexpr CGFloat kTitleViewVerticalSpacing = 2.0f;

// Creates the prompt label displaying `siteURL`.
UILabel* CreatePromptLabel(NSString* siteURL) {
  UILabel* promptLabel = [[UILabel alloc] init];
  promptLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  promptLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  promptLabel.textAlignment = NSTextAlignmentCenter;
  promptLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  promptLabel.adjustsFontForContentSizeCategory = YES;
  promptLabel.text = siteURL;
  [promptLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [promptLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  return promptLabel;
}

// Creates the title label displaying `title`.
UILabel* CreateTitleLabel(NSString* title) {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.text = title;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = kTitleLabelMinimumScaleFactor;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  return titleLabel;
}

}  // namespace

UIView* CreatePageInfoNavigationItemTitleView(NSString* title,
                                              NSString* siteURL) {
  const BOOL hasTitle = title.length > 0;
  const BOOL hasSiteURL = siteURL.length > 0;

  if (!hasTitle && !hasSiteURL) {
    return nil;
  }
  if (!hasSiteURL) {
    return CreateTitleLabel(title);
  }
  if (!hasTitle) {
    return CreatePromptLabel(siteURL);
  }

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    CreatePromptLabel(siteURL), CreateTitleLabel(title)
  ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.alignment = UIStackViewAlignmentFill;
  stackView.distribution = UIStackViewDistributionEqualSpacing;
  stackView.spacing = kTitleViewVerticalSpacing;
  return stackView;
}
