// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/reminder_notifications/ui/constants.h"
#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_date_picker_table_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size of the bell badge symbol.
constexpr CGFloat kBellBadgeSymbolPointSize = 60;

// Size of the frame holding the symbol.
constexpr CGFloat kCustomFaviconSideLength = 42;

// Spacing before the image when there's no navigation bar.
constexpr CGFloat kSpacingBeforeImage = 16;

// Spacing after the image before the title.
constexpr CGFloat kSpacingAfterImage = 16;

// Accessibility identifier for the bell icon.
NSString* const kBellIconAccessibilityLabel =
    @"ReminderNotificationsBellIconAccessibilityLabel";

}  // namespace

@interface ReminderNotificationsViewController () <
    UIAdaptivePresentationControllerDelegate,
    UIPopoverPresentationControllerDelegate>
@end

@implementation ReminderNotificationsViewController {
  UIViewController* _presentingPickerViewController;
  ReminderNotificationsDatePickerTableView* _tableView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Configure strings and layout before calling super to ensure proper setup.
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_SHEET_TITLE);
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_DESCRIPTION);

  // Set up date/time picker.
  _tableView = [[ReminderNotificationsDatePickerTableView alloc]
      initWithInteractionHandler:self];
  self.underTitleView = _tableView;

  // Set up action buttons.
  self.configuration.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_REMINDER_NOTIFICATIONS_SET_REMINDER_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_REMINDER_NOTIFICATIONS_CANCEL_BUTTON);

  // Configure title spacing.
  self.titleTextStyle = UIFontTextStyleTitle2;

  // Configure image.
  self.image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kBellBadgeSymbol, kBellBadgeSymbolPointSize),
      @[ [UIColor whiteColor] ]);
  self.imageBackgroundColor = [UIColor colorNamed:kBlue500Color];
  self.imageViewAccessibilityLabel = kBellIconAccessibilityLabel;
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImage;
  self.customSpacingAfterImage = kSpacingAfterImage;
  self.imageHasFixedSize = YES;
  self.imageEnclosedWithShadowWithoutBadge = YES;
  self.customFaviconSideLength = kCustomFaviconSideLength;
  self.alwaysShowImage = YES;

  // Configure layout preferences.
  self.topAlignedLayout = YES;

  [super viewDidLoad];

  [self.underTitleView.widthAnchor
      constraintEqualToAnchor:self.primaryActionButton.widthAnchor]
      .active = YES;
}

- (NSDate*)date {
  return _tableView.date;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

// Ensures the picker is always displayed in popover overlay, instead of falling
// back to a form sheet.
- (UIModalPresentationStyle)
    adaptivePresentationStyleForPresentationController:
        (UIPresentationController*)controller
                                       traitCollection:
                                           (UITraitCollection*)traitCollection {
  return UIModalPresentationNone;
}

#pragma mark - UIPopoverPresentationControllerDelegate

- (void)popoverPresentationController:
            (UIPopoverPresentationController*)popoverPresentationController
          willRepositionPopoverToRect:(inout CGRect*)rect
                               inView:(inout UIView**)view {
  // On devices with limited vertical space (compact height), position the
  // popover to the right of the row.
  popoverPresentationController.permittedArrowDirections =
      !IsCompactHeight(self.traitCollection) ? UIPopoverArrowDirectionDown
                                             : UIPopoverArrowDirectionLeft;

  // Popover moved to a different location, there might be more space available
  // now so a new layout pass is needed.
  [self.view setNeedsLayout];
}

- (void)popoverPresentationControllerDidDismissPopover:
    (UIPopoverPresentationController*)popoverPresentationController {
  _presentingPickerViewController = nil;
}

#pragma mark - ReminderNotificationsDatePickerInteractionHandler

// Presents a date/time picker as a popover from the specified view.
// Configures picker appearance and behavior based on mode (date or time)
// with appropriate visual styling and blur effect background.
- (void)showDatePickerFromOriginView:(UIView*)view
                  withDatePickerMode:(UIDatePickerMode)mode {
  if (_presentingPickerViewController) {
    return;
  }

  // Create and configure the date picker
  UIDatePicker* datePicker = [[UIDatePicker alloc] init];
  datePicker.datePickerMode = mode;
  datePicker.minimumDate = [NSDate date];
  datePicker.date = _tableView.date;

  [datePicker addTarget:self
                 action:@selector(datePickerValueChanged:)
       forControlEvents:UIControlEventValueChanged];

  // Set the preferred style based on the mode
  switch (mode) {
    case UIDatePickerModeDate:
      datePicker.preferredDatePickerStyle = UIDatePickerStyleInline;
      break;
    case UIDatePickerModeTime:
      datePicker.preferredDatePickerStyle = UIDatePickerStyleWheels;
      break;
    default:
      NOTREACHED();
  }

  // Create a view controller to contain the date picker
  CHECK(_presentingPickerViewController == nil);
  _presentingPickerViewController = [[UIViewController alloc] init];

  // Create blur effect
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterial];
  UIVisualEffectView* blurEffectView =
      [[UIVisualEffectView alloc] initWithEffect:blurEffect];
  blurEffectView.frame = CGRectZero;
  blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add blur view to the view controller's view
  [_presentingPickerViewController.view addSubview:blurEffectView];
  AddSameConstraints(blurEffectView, _presentingPickerViewController.view);

  // Add the date picker to the blur effect view's content view
  [blurEffectView.contentView addSubview:datePicker];
  datePicker.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraintsWithInset(datePicker, blurEffectView.contentView,
                              kReminderNotificationsDatePickerPadding);

  // Set appropriate size for the popover based on picker mode
  CGFloat height;
  switch (mode) {
    case UIDatePickerModeDate:
      height = kReminderNotificationsDatePickerHeightDate;
      break;
    case UIDatePickerModeTime:
      height = kReminderNotificationsDatePickerHeightTime;
      break;
    default:
      NOTREACHED();
  }

  _presentingPickerViewController.preferredContentSize =
      CGSizeMake(kReminderNotificationsDatePickerWidth, height);

  // Configure as a popover
  _presentingPickerViewController.modalPresentationStyle =
      UIModalPresentationPopover;

  // Create a presentation controller delegate to prevent adaptive presentation
  _presentingPickerViewController.presentationController.delegate = self;

  // Configure the popover position
  UIPopoverPresentationController* popoverController =
      _presentingPickerViewController.popoverPresentationController;
  popoverController.delegate = self;
  popoverController.canOverlapSourceViewRect = YES;
  popoverController.permittedArrowDirections =
      !IsCompactHeight(self.traitCollection) ? UIPopoverArrowDirectionDown
                                             : UIPopoverArrowDirectionLeft;

  // Use nil for default transparent background to let blur effect show through
  popoverController.backgroundColor = nil;
  popoverController.sourceView = view;

  // Present the popover
  [self presentViewController:_presentingPickerViewController
                     animated:YES
                   completion:nil];
}

#pragma mark - Private

- (void)datePickerValueChanged:(UIDatePicker*)picker {
  _tableView.date = picker.date;

  [_tableView reloadData];

  // Automatically dismisses the date picker (but not time picker) because date
  // selection has natural UX friction against accidental taps, while time
  // picker requires wheel scrolling where auto-dismissal would disrupt the
  // selection experience.
  if (picker.datePickerMode == UIDatePickerModeDate) {
    [_presentingPickerViewController dismissViewControllerAnimated:YES
                                                        completion:nil];
    _presentingPickerViewController = nil;
  }
}

@end
