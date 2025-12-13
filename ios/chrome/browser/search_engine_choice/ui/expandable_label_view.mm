// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui/expandable_label_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Returns a snippet label.
UILabel* SnippetLabel() {
  UILabel* snippetLabel = [[UILabel alloc] init];
  snippetLabel.translatesAutoresizingMaskIntoConstraints = NO;
  snippetLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  snippetLabel.adjustsFontForContentSizeCategory = YES;
  snippetLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  // Make sure the snippet is not streched.
  [snippetLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh + 1
                                  forAxis:UILayoutConstraintAxisVertical];
  return snippetLabel;
}

}  // namespace

@implementation ExpandableLabelView {
  // UILabel to display the first line of the snippet.
  UILabel* _oneLineLabel;
  // UILabel to display the snippet with all lines.
  UILabel* _multipleLineLabel;
  NSLayoutConstraint* _labelOneLineConstraint;
  NSLayoutConstraint* _labelMultipleLineConstraint;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _oneLineLabel = SnippetLabel();
    _oneLineLabel.numberOfLines = 1;
    [self addSubview:_oneLineLabel];
    _multipleLineLabel = SnippetLabel();
    _multipleLineLabel.numberOfLines = 0;
    [self addSubview:_multipleLineLabel];
    _labelOneLineConstraint =
        [_oneLineLabel.bottomAnchor constraintEqualToAnchor:self.bottomAnchor];
    _labelMultipleLineConstraint = [_multipleLineLabel.bottomAnchor
        constraintEqualToAnchor:self.bottomAnchor];
    NSArray* constraints = @[
      // One line label.
      [_oneLineLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_oneLineLabel.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_oneLineLabel.topAnchor constraintEqualToAnchor:self.topAnchor],
      // Expanded label.
      [_multipleLineLabel.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_multipleLineLabel.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_multipleLineLabel.topAnchor constraintEqualToAnchor:self.topAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
    [self updateLabels];
  }
  return self;
}

#pragma mark - Properties

- (void)setText:(NSString*)text {
  _text = [text copy];
  _multipleLineLabel.text = _text;
  _oneLineLabel.text = _text;
}

- (void)setExpanded:(BOOL)expanded {
  if (expanded == _expanded) {
    return;
  }
  _expanded = expanded;
  [self updateLabels];
}

- (NSLayoutYAxisAnchor*)oneLineBottomAnchor {
  return _oneLineLabel.bottomAnchor;
}

- (BOOL)isExpandable {
  [self layoutIfNeeded];
  // To know if this label is expandabled, we need to compare
  // `_multipleLineLabel` height and `_oneLineLabel` height. To avoid any issues
  // with float approximations, there is one pixel margin.
  return _multipleLineLabel.frame.size.height -
             _oneLineLabel.frame.size.height >
         1.;
}

#pragma mark - Private

// Updates the labels based on `_expanded` value.
- (void)updateLabels {
  _oneLineLabel.alpha = _expanded ? 0 : 1;
  _multipleLineLabel.alpha = _expanded ? 1 : 0;
  _labelOneLineConstraint.active = !_expanded;
  _labelMultipleLineConstraint.active = _expanded;
}

@end
