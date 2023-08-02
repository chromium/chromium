// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/share_extension_view.h"

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/share_extension/ui_util.h"

namespace {

const CGFloat kCornerRadius = 6;
// Minimum size around the widget
const CGFloat kDividerHeight = 0.5;
const CGFloat kShareExtensionPadding = 16;
const CGFloat kButtonHeight = 44;

// Size of the icon if present.
const CGFloat kScreenshotSize = 80;

// Size for the buttons font.
const CGFloat kButtonFontSize = 17;

}  // namespace

#pragma mark - Share Extension Button

// UIButton with the background color changing when it is highlighted.
@interface ShareExtensionButton : UIButton
@end

@implementation ShareExtensionButton

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];

  if (highlighted)
    self.backgroundColor = [UIColor colorNamed:kTableViewRowHighlightColor];
  else
    self.backgroundColor = UIColor.clearColor;
}

@end

#pragma mark - Share Extension View

@interface ShareExtensionView ()

// Keep strong references of the views that need to be updated.
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* URLLabel;
@property(nonatomic, strong) UIView* titleURLContainer;
@property(nonatomic, strong) UIButton* readingListButton;
@property(nonatomic, strong) UIImageView* screenshotView;
@property(nonatomic, strong) UIView* itemView;

@property(nonatomic, weak) id<ShareExtensionViewActionTarget> target;

// Track if a button has been pressed. All button pressing will have no effect
// if `dismissed` is YES.
@property(nonatomic, assign) BOOL dismissed;

@end

@implementation ShareExtensionView

#pragma mark - Lifecycle

- (instancetype)initWithActionTarget:
    (id<ShareExtensionViewActionTarget>)target {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(target);
    _target = target;

    [self.layer setCornerRadius:kCornerRadius];
    [self setClipsToBounds:YES];

    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];

    NSString* addToReadingListTitle = NSLocalizedString(
        @"IDS_IOS_ADD_READING_LIST_SHARE_EXTENSION",
        @"The add to reading list button text in share extension.");
    self.readingListButton =
        [self buttonWithTitle:addToReadingListTitle
                     selector:@selector(addToReadingListPressed:)];

    NSString* addToBookmarksTitle = NSLocalizedString(
        @"IDS_IOS_ADD_BOOKMARKS_SHARE_EXTENSION",
        @"The Add to bookmarks button text in share extension.");
    UIButton* bookmarksButton =
        [self buttonWithTitle:addToBookmarksTitle
                     selector:@selector(addToBookmarksPressed:)];

    NSString* openInChromeTitle = NSLocalizedString(
        @"IDS_IOS_OPEN_IN_CHROME_SHARE_EXTENSION",
        @"The Open in Chrome button text in share extension.");
    UIButton* openButton =
        [self buttonWithTitle:openInChromeTitle
                     selector:@selector(openInChromePressed:)];

    for (UIButton* button in
         @[ self.readingListButton, bookmarksButton, openButton ]) {
      button.pointerInteractionEnabled = YES;
      button.pointerStyleProvider = ^UIPointerStyle*(
          UIButton* theButton, __unused UIPointerEffect* proposedEffect,
          __unused UIPointerShape* proposedShape) {
        UITargetedPreview* preview =
            [[UITargetedPreview alloc] initWithView:theButton];
        UIPointerHoverEffect* effect =
            [UIPointerHoverEffect effectWithPreview:preview];
        return [UIPointerStyle styleWithEffect:effect shape:nil];
      };
    }

    UIStackView* contentStack = [[UIStackView alloc] initWithArrangedSubviews:@[
      [self navigationBar], [self dividerView], [self sharedItemView],
      [self dividerView], self.readingListButton, [self dividerView],
      bookmarksButton, [self dividerView], openButton
    ]];
    [contentStack setAxis:UILayoutConstraintAxisVertical];
    [self addSubview:contentStack];

    [contentStack setTranslatesAutoresizingMaskIntoConstraints:NO];

    ui_util::ConstrainAllSidesOfViewToView(self, contentStack);
  }
  return self;
}

#pragma mark Init helpers

// Returns a view containing the shared items (title, URL, screenshot). This
// method will set the ivars.
- (UIView*)sharedItemView {
  // Title label. Text will be filled by `setTitle:` when available.
  _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _titleLabel.font = [UIFont boldSystemFontOfSize:16];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  // URL label. Text will be filled by `setURL:` when available.
  _URLLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  _URLLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _URLLabel.numberOfLines = 3;
  _URLLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _URLLabel.font = [UIFont systemFontOfSize:12];
  _URLLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];

  // Screenshot view. Image will be filled by `setScreenshot:` when available.
  _screenshotView = [[UIImageView alloc] initWithFrame:CGRectZero];
  [_screenshotView setTranslatesAutoresizingMaskIntoConstraints:NO];
  NSLayoutConstraint* imageWidthConstraint =
      [_screenshotView.widthAnchor constraintEqualToConstant:0];
  imageWidthConstraint.priority = UILayoutPriorityDefaultHigh;
  imageWidthConstraint.active = YES;

  [_screenshotView.heightAnchor
      constraintEqualToAnchor:_screenshotView.widthAnchor]
      .active = YES;
  [_screenshotView setContentMode:UIViewContentModeScaleAspectFill];
  [_screenshotView setClipsToBounds:YES];

  // `_screenshotView` should take as much space as needed. Lower compression
  // resistance of the other elements.
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                 forAxis:UILayoutConstraintAxisVertical];
  [_URLLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                               forAxis:UILayoutConstraintAxisVertical];

  [_URLLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _titleURLContainer = [[UIView alloc] initWithFrame:CGRectZero];
  [_titleURLContainer setTranslatesAutoresizingMaskIntoConstraints:NO];

  [_titleURLContainer addSubview:_titleLabel];
  [_titleURLContainer addSubview:_URLLabel];

  _itemView = [[UIView alloc] init];
  [_itemView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [_itemView addSubview:_titleURLContainer];
  [_itemView addSubview:_screenshotView];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.topAnchor
        constraintEqualToAnchor:_titleURLContainer.topAnchor],
    [_URLLabel.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor],
    [_URLLabel.bottomAnchor
        constraintEqualToAnchor:_titleURLContainer.bottomAnchor],
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:_titleURLContainer.trailingAnchor],
    [_URLLabel.trailingAnchor
        constraintEqualToAnchor:_titleURLContainer.trailingAnchor],
    [_titleLabel.leadingAnchor
        constraintEqualToAnchor:_titleURLContainer.leadingAnchor],
    [_URLLabel.leadingAnchor
        constraintEqualToAnchor:_titleURLContainer.leadingAnchor],
    [_titleURLContainer.centerYAnchor
        constraintEqualToAnchor:_itemView.centerYAnchor],
    [_itemView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_titleURLContainer.heightAnchor
                                    constant:2 * kShareExtensionPadding],
    [_titleURLContainer.leadingAnchor
        constraintEqualToAnchor:_itemView.leadingAnchor
                       constant:kShareExtensionPadding],
    [_screenshotView.trailingAnchor
        constraintEqualToAnchor:_itemView.trailingAnchor
                       constant:-kShareExtensionPadding],
    [_itemView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_screenshotView.heightAnchor
                                    constant:2 * kShareExtensionPadding],
    [_screenshotView.centerYAnchor
        constraintEqualToAnchor:_itemView.centerYAnchor],
  ]];

  NSLayoutConstraint* titleURLScreenshotConstraint =
      [_titleURLContainer.trailingAnchor
          constraintEqualToAnchor:_screenshotView.leadingAnchor];
  titleURLScreenshotConstraint.priority = UILayoutPriorityDefaultHigh;
  titleURLScreenshotConstraint.active = YES;

  return _itemView;
}

// Returns a view containing a divider.
- (UIView*)dividerView {
  UIView* divider = [[UIView alloc] initWithFrame:CGRectZero];
  [divider setTranslatesAutoresizingMaskIntoConstraints:NO];
  divider.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  CGFloat slidingConstant = AlignValueToPixel(kDividerHeight);
  [divider.heightAnchor constraintEqualToConstant:slidingConstant].active = YES;
  return divider;
}

// Returns a button containing title `title` and action `selector` on
// `self.target`.
- (UIButton*)buttonWithTitle:(NSString*)title selector:(SEL)selector {
  UIButton* button = [[ShareExtensionButton alloc] initWithFrame:CGRectZero];
  [button setTitle:title forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  [[button titleLabel] setFont:[UIFont systemFontOfSize:kButtonFontSize]];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [button addTarget:self
                action:selector
      forControlEvents:UIControlEventTouchUpInside];
  [button.heightAnchor constraintEqualToConstant:kButtonHeight].active = YES;
  return button;
}

// Returns a navigationBar.
- (UINavigationBar*)navigationBar {
  // Create the navigation bar.
  UINavigationBar* navigationBar =
      [[UINavigationBar alloc] initWithFrame:CGRectZero];
  [[navigationBar layer] setCornerRadius:kCornerRadius];
  [navigationBar setClipsToBounds:YES];

  // Create an empty image to replace the standard gray background of the
  // UINavigationBar.
  UIImage* emptyImage = [[UIImage alloc] init];
  [navigationBar setBackgroundImage:emptyImage
                      forBarMetrics:UIBarMetricsDefault];
  [navigationBar setShadowImage:emptyImage];
  [navigationBar setTranslucent:YES];
  [navigationBar setTranslatesAutoresizingMaskIntoConstraints:NO];
  navigationBar.titleTextAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor]
  };

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelPressed:)];

  NSString* appName = [base::apple::FrameworkBundle()
      objectForInfoDictionaryKey:@"CFBundleDisplayName"];
  UINavigationItem* titleItem =
      [[UINavigationItem alloc] initWithTitle:appName];
  [titleItem setLeftBarButtonItem:cancelButton];
  [titleItem setHidesBackButton:YES];
  [navigationBar pushNavigationItem:titleItem animated:NO];
  return navigationBar;
}

// Called when "Add to Reading List" button has been pressed.
- (void)addToReadingListPressed:(UIButton*)sender {
  if (self.dismissed) {
    return;
  }
  self.dismissed = YES;
  [self
      animateButtonPressed:sender
            withCompletion:^{
              [self.target shareExtensionViewDidSelectAddToReadingList:sender];
            }];
}

// Called when "Add to bookmarks" button has been pressed.
- (void)addToBookmarksPressed:(UIButton*)sender {
  if (self.dismissed) {
    return;
  }
  self.dismissed = YES;
  [self animateButtonPressed:sender
              withCompletion:^{
                [self.target shareExtensionViewDidSelectAddToBookmarks:sender];
              }];
}

- (void)openInChromePressed:(UIButton*)sender {
  if (self.dismissed) {
    return;
  }
  self.dismissed = YES;
  [self.target shareExtensionViewDidSelectOpenInChrome:sender];
}

// Animates the button `sender` by replacing its string to "Added", then call
// completion.
- (void)animateButtonPressed:(UIButton*)sender
              withCompletion:(void (^)(void))completion {
  NSString* addedString =
      NSLocalizedString(@"IDS_IOS_ADDED_ITEM_SHARE_EXTENSION",
                        @"Button label after being pressed.");
  NSString* addedCheckedString =
      [addedString stringByAppendingString:@" \u2713"];
  // Create a label with the same style as the split animation between the text
  // and the checkmark.
  UILabel* addedLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [addedLabel setTranslatesAutoresizingMaskIntoConstraints:NO];
  [addedLabel setText:addedString];
  [self addSubview:addedLabel];
  [addedLabel setFont:[sender titleLabel].font];
  [addedLabel setTextColor:[sender titleColorForState:UIControlStateNormal]];
  [addedLabel.leadingAnchor
      constraintEqualToAnchor:[sender titleLabel].leadingAnchor]
      .active = YES;
  [addedLabel.centerYAnchor
      constraintEqualToAnchor:[sender titleLabel].centerYAnchor]
      .active = YES;
  [addedLabel setAlpha:0];

  void (^step3ShowCheck)() = ^{
    [UIView animateWithDuration:ui_util::kAnimationDuration
        animations:^{
          [addedLabel setAlpha:0];
          [sender setAlpha:1];
        }
        completion:^(BOOL finished) {
          if (completion) {
            completion();
          }
        }];
  };

  void (^step2ShowTextWithoutCheck)() = ^{
    [sender setTitle:addedCheckedString forState:UIControlStateNormal];
    [UIView animateWithDuration:ui_util::kAnimationDuration
        animations:^{
          [addedLabel setAlpha:1];
        }
        completion:^(BOOL finished) {
          step3ShowCheck();
        }];
  };

  void (^step1HideText)() = ^{
    [UIView animateWithDuration:ui_util::kAnimationDuration
        animations:^{
          [sender setAlpha:0];
        }
        completion:^(BOOL finished) {
          step2ShowTextWithoutCheck();
        }];
  };
  step1HideText();
}

// Called when "Cancel" button has been pressed.
- (void)cancelPressed:(UIButton*)sender {
  if (self.dismissed) {
    return;
  }
  self.dismissed = YES;
  [self.target shareExtensionViewDidSelectCancel:sender];
}

#pragma mark - Content getters and setters.

- (void)setURL:(NSURL*)URL {
  [[self URLLabel] setText:[URL absoluteString]];
}

- (void)setTitle:(NSString*)title {
  [[self titleLabel] setText:title];
}

- (void)setScreenshot:(UIImage*)screenshot {
  [self.screenshotView.widthAnchor constraintEqualToConstant:kScreenshotSize]
      .active = YES;
  [self.titleURLContainer.trailingAnchor
      constraintEqualToAnchor:self.screenshotView.leadingAnchor
                     constant:-kShareExtensionPadding]
      .active = YES;
  [[self screenshotView] setImage:screenshot];
}

@end
