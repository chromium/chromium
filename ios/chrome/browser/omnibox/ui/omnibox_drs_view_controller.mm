// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/omnibox_drs_view_controller.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@interface OmniboxDRSViewController ()

@property(nonatomic, strong) UIView* popupContainerView;
@property(nonatomic, strong) UIView* webViewPlaceholder;
@property(nonatomic, strong) UIScrollView* contentView;

@end

// NewTabPrototypeViewController

@implementation OmniboxDRSViewController

- (instancetype)init {
  self = [super init];
  if (self) {
    _popupContainerView = [[UIView alloc] init];
    _webViewPlaceholder = [[UIView alloc] init];
    _webViewPlaceholder.backgroundColor = UIColor.greenColor;
    _webViewPlaceholder.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return self;
}

- (void)viewDidLoad {
  self.view.backgroundColor = UIColor.lightGrayColor;

  UIView* inputView = [self createInputView];
  [self.view addSubview:inputView];

  inputView.translatesAutoresizingMaskIntoConstraints = NO;
  [inputView setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisVertical];
  [NSLayoutConstraint activateConstraints:@[
    [inputView.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor],
    [inputView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [inputView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  self.contentView = [self createContentView];
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:self.contentView];

  [NSLayoutConstraint activateConstraints:@[

    [self.contentView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.contentView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    [self.contentView.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:inputView.topAnchor],

    [self.contentView.topAnchor constraintEqualToAnchor:self.view.topAnchor],

  ]];

  UIButton* exitButton = [self createExitButton];
  [self.view addSubview:exitButton];
  exitButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [exitButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [exitButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [exitButton.widthAnchor constraintEqualToConstant:40.0f],
    [exitButton.heightAnchor constraintEqualToConstant:40.0f],
  ]];
}

- (UIScrollView*)createContentView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];

  scrollView.pagingEnabled = YES;
  self.popupContainerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    self.popupContainerView, self.webViewPlaceholder
  ]];
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:stackView];

  [NSLayoutConstraint activateConstraints:@[

    [self.popupContainerView.widthAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor],
    [self.popupContainerView.heightAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.heightAnchor],
    [self.popupContainerView.bottomAnchor
        constraintEqualToAnchor:stackView.bottomAnchor],
    [self.popupContainerView.topAnchor
        constraintEqualToAnchor:stackView.topAnchor],

    [self.webViewPlaceholder.widthAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.widthAnchor],
    [self.webViewPlaceholder.heightAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.heightAnchor],
    [self.webViewPlaceholder.bottomAnchor
        constraintEqualToAnchor:stackView.bottomAnchor],
    [self.webViewPlaceholder.topAnchor
        constraintEqualToAnchor:stackView.topAnchor],
  ]];

  [NSLayoutConstraint activateConstraints:@[

    [stackView.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor],
    [stackView.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor],
    [stackView.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor],
    [stackView.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor],
  ]];

  return scrollView;
}

- (UIButton*)createExitButton {
  UIImage* xImage = DefaultSymbolWithConfiguration(@"x.circle.fill", nil);
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.image = xImage;
  config.baseBackgroundColor = [UIColor colorWithRed:1.0
                                               green:0.0
                                                blue:0.
                                               alpha:0.7];

  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf close];
  }];

  return [UIButton buttonWithConfiguration:config primaryAction:closeAction];
}

- (UIView*)createInputView {
  UIView* view = [[UIView alloc] init];
  view.backgroundColor = UIColor.systemPinkColor;

  UITextView* textView = [[UITextView alloc] init];
  textView.backgroundColor = UIColor.yellowColor;
  textView.scrollEnabled = NO;

  textView.translatesAutoresizingMaskIntoConstraints = NO;

  UISegmentedControl* segmentedControl =
      [[UISegmentedControl alloc] initWithItems:@[ @"Search", @"AI Mode" ]];
  segmentedControl.selectedSegmentIndex = 0;
  [segmentedControl addTarget:self
                       action:@selector(segmentedControlValueChanged:)
             forControlEvents:UIControlEventValueChanged];

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ textView, segmentedControl ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  [stackView setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisVertical];

  [view addSubview:stackView];

  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [segmentedControl.heightAnchor constraintEqualToConstant:38.0f],
    [segmentedControl.leadingAnchor
        constraintEqualToAnchor:stackView.leadingAnchor],
    [segmentedControl.trailingAnchor
        constraintEqualToAnchor:stackView.trailingAnchor],

  ]];
  [NSLayoutConstraint activateConstraints:@[
    [textView.heightAnchor constraintGreaterThanOrEqualToConstant:38.0f],
    [textView.leadingAnchor constraintEqualToAnchor:stackView.leadingAnchor],
    [textView.trailingAnchor constraintEqualToAnchor:stackView.trailingAnchor],
    [textView.heightAnchor constraintLessThanOrEqualToConstant:200.0f],
  ]];
  [textView setContentHuggingPriority:UILayoutPriorityDefaultHigh
                              forAxis:UILayoutConstraintAxisVertical];

  [NSLayoutConstraint activateConstraints:@[
    [stackView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [stackView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [stackView.topAnchor constraintEqualToAnchor:view.topAnchor],
    [stackView.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],

  ]];

  return view;
}

#pragma mark - UIActions

- (void)close {
  [self.presentingViewController dismissViewControllerAnimated:YES
                                                    completion:nil];
}

- (void)segmentedControlValueChanged:(UISegmentedControl*)sender {
  CGRect frame = self.contentView.frame;
  self.contentView.contentOffset =
      CGPointMake(frame.size.width * sender.selectedSegmentIndex, 0);
}

#pragma mark - OmniboxPopupPresenterDelegate

- (UIView*)popupParentViewForPresenter:(OmniboxPopupPresenter*)presenter {
  return self.popupContainerView;
}

- (UIViewController*)popupParentViewControllerForPresenter:
    (OmniboxPopupPresenter*)presenter {
  return self;
}

- (UIColor*)popupBackgroundColorForPresenter:(OmniboxPopupPresenter*)presenter {
  return [self.proxiedPresenterDelegate
      popupBackgroundColorForPresenter:presenter];
}

- (GuideName*)omniboxGuideNameForPresenter:(OmniboxPopupPresenter*)presenter {
  return [self.proxiedPresenterDelegate omniboxGuideNameForPresenter:presenter];
}

- (void)popupDidOpenForPresenter:(OmniboxPopupPresenter*)presenter {
  [self.proxiedPresenterDelegate popupDidOpenForPresenter:presenter];
}

- (void)popupDidCloseForPresenter:(OmniboxPopupPresenter*)presenter {
  [self.proxiedPresenterDelegate popupDidCloseForPresenter:presenter];
  [self close];
}

@end
