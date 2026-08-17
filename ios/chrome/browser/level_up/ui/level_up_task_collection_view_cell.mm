// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_task_collection_view_cell.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/ui/level_up_completed_task_header_row_view.h"
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

@interface LevelUpTaskCollectionViewCell () <LevelUpTaskRowViewDelegate>
@end

@implementation LevelUpTaskCollectionViewCell {
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
    self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self.contentView, self);

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

    UIView* stretchSpacer = [[UIView alloc] init];
    stretchSpacer.translatesAutoresizingMaskIntoConstraints = NO;
    [stretchSpacer setContentHuggingPriority:1
                                     forAxis:UILayoutConstraintAxisVertical];
    [stretchSpacer
        setContentCompressionResistancePriority:1
                                        forAxis:UILayoutConstraintAxisVertical];

    UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
      headerView, separatorView, _rowsStackView, stretchSpacer
    ]];
    mainStack.axis = UILayoutConstraintAxisVertical;
    mainStack.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:mainStack];

    AddSameConstraintsToSides(mainStack, self.contentView,
                              LayoutSides::kTop | LayoutSides::kHorizontal);
    NSLayoutConstraint* stackBottomConstraint = [mainStack.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor];
    stackBottomConstraint.priority = UILayoutPriorityDefaultHigh - 1;
    stackBottomConstraint.active = YES;
  }
  return self;
}

#pragma mark - UICollectionViewCell

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
  [_rowsStackView.arrangedSubviews
      makeObjectsPerformSelector:@selector(removeFromSuperview)];
}

- (UICollectionViewLayoutAttributes*)preferredLayoutAttributesFittingAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  UICollectionViewLayoutAttributes* attributes =
      [super preferredLayoutAttributesFittingAttributes:layoutAttributes];
  CGSize fittingSize = [self.contentView
      systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  CGRect cellFrame = layoutAttributes.frame;
  cellFrame.size.height = fittingSize.height;
  attributes.frame = cellFrame;
  return attributes;
}

- (void)setTasks:(NSArray<LevelUpTask*>*)activeTasks
       completedTasks:(NSArray<LevelUpTask*>*)completedTasks
    completedExpanded:(BOOL)completedExpanded {
  [_rowsStackView.arrangedSubviews
      makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [self addRows:activeTasks hidden:NO];

  if (completedTasks.count > 0) {
    [self addHeaderWithCount:completedTasks.count expanded:completedExpanded];
    [self addRows:completedTasks hidden:!completedExpanded];
  }
}

#pragma mark - Private

// Create and add task rows to the stack view.
- (void)addRows:(NSArray<LevelUpTask*>*)tasks hidden:(BOOL)hidden {
  for (LevelUpTask* task in tasks) {
    LevelUpTaskRowView* row = [[LevelUpTaskRowView alloc] init];
    row.delegate = self;
    BOOL showSeparator = (task != tasks.lastObject);
    [row configureWithTask:task showSeparator:showSeparator];
    row.hidden = hidden;
    [_rowsStackView addArrangedSubview:row];
  }
}

// Add the completed tasks header row to the stack view.
- (void)addHeaderWithCount:(NSInteger)count expanded:(BOOL)expanded {
  LevelUpCompletedTaskHeaderRowView* header =
      [[LevelUpCompletedTaskHeaderRowView alloc] init];
  [header configureWithCompletedTasksCount:count expanded:expanded];
  [header addTarget:self
                action:@selector(didTapCompletedHeader)
      forControlEvents:UIControlEventTouchUpInside];
  [_rowsStackView addArrangedSubview:header];
}

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

// Target executed when user taps the "See All" action link.
- (void)didTapSeeAll {
  [self.delegate didTapSeeAllTasks:self];
}

// Target executed when user taps the completed header.
- (void)didTapCompletedHeader {
  [self.delegate taskCollectionViewDidTapCompletedHeader:self];
}

#pragma mark - LevelUpTaskRowViewDelegate

- (void)taskRowView:(LevelUpTaskRowView*)rowView didTapTask:(LevelUpTask*)task {
  [self.delegate taskCollectionViewCell:self didTapTask:task];
}

@end
