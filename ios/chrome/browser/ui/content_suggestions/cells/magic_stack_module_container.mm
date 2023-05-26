// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The horizontal inset for the content within this container.
const CGFloat kContentHorizontalInset = 16.0f;

// The top inset for the content within this container.
const CGFloat kContentTopInset = 14.0f;

// The bottom inset for the content within this container.
const CGFloat kContentBottomInset = 10.0f;

// Vertical spacing between the content views.
const float kContentVerticalSpacing = 12.0f;

// The corner radius of this container.
const float kCornerRadius = 24;

// The width of the modules.
const int kModuleWidthCompact = 343;
const int kModuleWidthRegular = 382;

}  // namespace

@interface MagicStackModuleContainer ()

// The type of this container.
@property(nonatomic, assign) ContentSuggestionsModuleType type;

@end

@implementation MagicStackModuleContainer {
  NSLayoutConstraint* _contentViewWidthAnchor;
  id<MagicStackModuleContainerDelegate> _delegate;
}

- (instancetype)initWithType:(ContentSuggestionsModuleType)type {
  self = [super initWithFrame:CGRectZero];
  if (self) {
  }
  return self;
}

- (instancetype)initWithContentView:(UIView*)contentView
                               type:(ContentSuggestionsModuleType)type
                           delegate:
                               (id<MagicStackModuleContainerDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _type = type;
    _delegate = delegate;
    self.layer.cornerRadius = kCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

    UILabel* title = [[UILabel alloc] init];
    title.text = [MagicStackModuleContainer titleStringForModule:type];
    title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    title.textColor = [UIColor colorNamed:kTextPrimaryColor];
    title.accessibilityTraits |= UIAccessibilityTraitHeader;
    title.accessibilityIdentifier =
        [MagicStackModuleContainer titleStringForModule:type];

    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.alignment = UIStackViewAlignmentLeading;
    stackView.axis = UILayoutConstraintAxisVertical;
    stackView.spacing = kContentVerticalSpacing;
    stackView.distribution = UIStackViewDistributionFill;
    [stackView addArrangedSubview:title];
    [stackView addArrangedSubview:contentView];

    self.accessibilityElements = @[ title, contentView ];

    _contentViewWidthAnchor = [contentView.widthAnchor
        constraintEqualToConstant:[self contentViewWidth]];
    [NSLayoutConstraint activateConstraints:@[ _contentViewWidthAnchor ]];

    [self addSubview:stackView];
    AddSameConstraintsWithInsets(stackView, self, [self contentMargins]);
  }
  return self;
}

+ (CGFloat)moduleWidthForHorizontalTraitCollection:
    (UITraitCollection*)traitCollection {
  return traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular
             ? kModuleWidthRegular
             : kModuleWidthCompact;
}

+ (NSString*)titleStringForModule:(ContentSuggestionsModuleType)type {
  switch (type) {
    case ContentSuggestionsModuleType::kShortcuts:
      return l10n_util::GetNSString(
          IDS_IOS_CONTENT_SUGGESTIONS_SHORTCUTS_MODULE_TITLE);
    case ContentSuggestionsModuleType::kMostVisited:
      if (ShouldPutMostVisitedSitesInMagicStack()) {
        return l10n_util::GetNSString(
            IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_TITLE);
      }
      return @"";
    case ContentSuggestionsModuleType::kSetUpListSync:
    case ContentSuggestionsModuleType::kSetUpListDefaultBrowser:
    case ContentSuggestionsModuleType::kSetUpListAutofill:
    case ContentSuggestionsModuleType::kCompactedSetUpList:
    case ContentSuggestionsModuleType::kSetUpListAllSet:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
    default:
      NOTREACHED();
      return @"";
  }
}

- (NSDirectionalEdgeInsets)contentMargins {
  NSDirectionalEdgeInsets contentMargins =
      NSDirectionalEdgeInsetsMake(kContentTopInset, kContentHorizontalInset,
                                  kContentBottomInset, kContentHorizontalInset);
  if (_type == ContentSuggestionsModuleType::kCompactedSetUpList) {
    contentMargins.trailing = 0;
  }
  return contentMargins;
}

- (CGSize)intrinsicContentSize {
  // When the Most Visited Tiles module is not in the Magic Stack or if a module
  // is the only module in the Magic Stack in a wider screen, the module should
  // be wider to match the wider Magic Stack ScrollView.
  BOOL MVTModuleShouldUseWideWidth =
      (_type == ContentSuggestionsModuleType::kMostVisited &&
       !ShouldPutMostVisitedSitesInMagicStack() &&
       self.traitCollection.horizontalSizeClass ==
           UIUserInterfaceSizeClassRegular);
  BOOL moduleShouldUseWideWidth =
      self.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassRegular &&
      [_delegate doesMagicStackShowOnlyOneModule:_type];
  if (MVTModuleShouldUseWideWidth || moduleShouldUseWideWidth) {
    return CGSizeMake(kMagicStackWideWidth, self.bounds.size.height);
  }
  return CGSizeMake(
      [MagicStackModuleContainer
          moduleWidthForHorizontalTraitCollection:self.traitCollection],
      self.bounds.size.height);
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.horizontalSizeClass !=
          self.traitCollection.horizontalSizeClass &&
      _type == ContentSuggestionsModuleType::kMostVisited &&
      !ShouldPutMostVisitedSitesInMagicStack()) {
    _contentViewWidthAnchor.constant = [self contentViewWidth];
  }
}

#pragma mark - Helpers

// Returns the expected width of the contentView subview.
- (CGFloat)contentViewWidth {
  NSDirectionalEdgeInsets insets = [self contentMargins];
  return [self intrinsicContentSize].width - insets.leading - insets.trailing;
}

@end
