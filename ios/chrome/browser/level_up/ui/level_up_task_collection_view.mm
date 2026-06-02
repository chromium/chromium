// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task_collection_view.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/ui/level_up_task_row_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The card corner radius.
const CGFloat kCardCornerRadius = 24.0;
// The opacity of the card shadow.
const CGFloat kCardShadowOpacity = 1.0;
// The blur radius of the card shadow.
const CGFloat kCardShadowRadius = 2.0;
// The vertical offset of the card shadow.
const CGFloat kCardShadowOffset = 1.0;
// The color alpha of the card shadow.
const CGFloat kCardShadowAlpha = 0.05;
// Unified spacing constant for padding and vertical stack views.
const CGFloat kLayoutSpacing = 16.0;
}  // namespace

@implementation LevelUpTaskCollectionView {
  UILabel* _titleLabel;
  UIButton* _seeAllButton;
  UIStackView* _rowsStackView;
}

@synthesize delegate = _delegate;

- (void)setHeaderTitle:(NSString*)headerTitle {
  _titleLabel.text = headerTitle;
}

- (NSString*)headerTitle {
  return _titleLabel.text;
}

- (void)setShowsSeeAllButton:(BOOL)showsSeeAllButton {
  _seeAllButton.hidden = !showsSeeAllButton;
}

- (BOOL)showsSeeAllButton {
  return !_seeAllButton.hidden;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
    self.contentView.layer.cornerRadius = kCardCornerRadius;
    self.contentView.layer.masksToBounds = YES;

    self.layer.shadowColor =
        [UIColor colorWithRed:0 green:0 blue:0 alpha:kCardShadowAlpha].CGColor;
    self.layer.shadowOpacity = kCardShadowOpacity;
    self.layer.shadowRadius = kCardShadowRadius;
    self.layer.shadowOffset = CGSizeMake(0, kCardShadowOffset);
    self.layer.masksToBounds = NO;

    UIView* headerView = [self createHeaderView];
    UIView* separatorView = [self createSeparatorView];
    _rowsStackView = [[UIStackView alloc] init];
    _rowsStackView.axis = UILayoutConstraintAxisVertical;
    _rowsStackView.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
      headerView, separatorView, _rowsStackView
    ]];
    mainStack.axis = UILayoutConstraintAxisVertical;
    mainStack.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:mainStack];
    AddSameConstraints(mainStack, self.contentView);
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.shadowPath =
      [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                 cornerRadius:kCardCornerRadius]
          .CGPath;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
  _titleLabel.text = nil;
  _seeAllButton.hidden = YES;
}

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  // Clear previous task rows.
  [_rowsStackView.arrangedSubviews
      makeObjectsPerformSelector:@selector(removeFromSuperview)];

  // Populates rows dynamically.
  for (NSUInteger i = 0; i < tasks.count; i++) {
    LevelUpTask* task = tasks[i];
    LevelUpTaskRowView* row = [[LevelUpTaskRowView alloc] init];
    BOOL showSeparator = i < tasks.count - 1;
    [row configureWithTask:task showSeparator:showSeparator];
    [_rowsStackView addArrangedSubview:row];
  }
}

#pragma mark - Private

// Creates the tasks card header row.
- (UIView*)createHeaderView {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  UIFont* subheadFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  UIFontDescriptor* titleBoldDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  _titleLabel.font = [UIFont fontWithDescriptor:titleBoldDescriptor size:0.0];

  _seeAllButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _seeAllButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_seeAllButton setTitle:l10n_util::GetNSString(IDS_IOS_LEVEL_UP_SEE_ALL)
                 forState:UIControlStateNormal];
  [_seeAllButton setTitleColor:[UIColor colorNamed:kBlueColor]
                      forState:UIControlStateNormal];
  _seeAllButton.titleLabel.font = [UIFont fontWithDescriptor:titleBoldDescriptor
                                                        size:0.0];
  [_seeAllButton addTarget:self
                    action:@selector(didTapSeeAll)
          forControlEvents:UIControlEventTouchUpInside];

  UIView* spacer = [[UIView alloc] init];
  [spacer setContentHuggingPriority:UILayoutPriorityDefaultLow
                            forAxis:UILayoutConstraintAxisHorizontal];

  UIStackView* headerStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _titleLabel, spacer, _seeAllButton ]];
  headerStack.axis = UILayoutConstraintAxisHorizontal;
  headerStack.alignment = UIStackViewAlignmentCenter;
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* headerPaddingContainer = [[UIView alloc] init];
  headerPaddingContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [headerPaddingContainer addSubview:headerStack];
  AddSameConstraintsWithInsets(
      headerStack, headerPaddingContainer,
      NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                  kLayoutSpacing, kLayoutSpacing));

  return headerPaddingContainer;
}

// Creates the card header thin separator line.
- (UIView*)createSeparatorView {
  UIView* separator = [[UIView alloc] init];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  [separator.heightAnchor constraintEqualToConstant:1.0].active = YES;
  return separator;
}

// Target helper executed when user taps the "See All" action link.
- (void)didTapSeeAll {
  [self.delegate didTapSeeAllTasks:self];
}

@end
