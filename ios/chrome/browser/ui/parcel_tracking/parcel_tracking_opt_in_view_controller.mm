// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller_delegate.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {
// Name of the parcel tracking icon.
NSString* const kOptInIcon = @"parcel_tracking_icon_new";
// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;
// Estimated row height for each cell in the table view.
CGFloat const kTableViewEstimatedRowHeight = 48;
// Margin for the options view.
CGFloat const kOptionsViewMargin = 17;
// Spacing before the image.
CGFloat const kSpacingBeforeImage = 23;
// Size of the radio buttons.
CGFloat const kRadioButtonSize = 20;
}  // namespace

@interface ParcelTrackingOptInViewController () <UITableViewDelegate,
                                                 UITextViewDelegate,
                                                 UITableViewDataSource>

@end

@implementation ParcelTrackingOptInViewController {
  UITableView* _tableView;
  IOSParcelTrackingOptInStatus _selection;
}

- (void)viewDidLoad {
  UIView* optionsView = [self createOptionsView];
  self.underTitleView = optionsView;
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_TITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_ENABLE_TRACKING);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SECONDARY_ACTION);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SUBTITLE);
  self.subtitleTextStyle = UIFontTextStyleCallout;
  self.showDismissBarButton = NO;
  self.image = [UIImage imageNamed:kOptInIcon];
  self.imageHasFixedSize = true;
  self.topAlignedLayout = YES;
  if (@available(iOS 16, *)) {
    self.sheetPresentationController.detents = @[
      UISheetPresentationControllerDetent.largeDetent,
      self.preferredHeightDetent
    ];
  }
  self.customSpacingAfterImage = 0;
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImage;
  [super viewDidLoad];

  // Assign table view's width anchor now that it is in the same hierarchy as
  // the top view.
  [NSLayoutConstraint activateConstraints:@[
    [optionsView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                              constant:kOptionsViewMargin],
    [optionsView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                               constant:-kOptionsViewMargin],
  ]];
  [self updateButtonForState:UIControlStateDisabled];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle,
  };
  NSDictionary* linkAttributes =
      @{NSLinkAttributeName : net::NSURLWithGURL(GURL("chrome://settings"))};
  subtitle.attributedText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SUBTITLE),
      textAttributes, linkAttributes);
  subtitle.delegate = self;
  subtitle.editable = YES;
  subtitle.selectable = YES;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  switch (_selection) {
    case IOSParcelTrackingOptInStatus::kAlwaysTrack:
      [self.delegate alwaysTrackTapped];
      break;
    case IOSParcelTrackingOptInStatus::kAskToTrack:
      [self.delegate askToTrackTapped];
      break;
    case IOSParcelTrackingOptInStatus::kNeverTrack:
      NOTREACHED();
      break;
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate noThanksTapped];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.delegate parcelTrackingSettingsPageLinkTapped];
  return NO;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 2;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewTextCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell"];

  NSString* title = indexPath.row == 0
                        ? l10n_util::GetNSString(
                              IDS_IOS_PARCEL_TRACKING_OPT_IN_PRIMARY_ACTION)
                        : l10n_util::GetNSString(
                              IDS_IOS_PARCEL_TRACKING_OPT_IN_TERTIARY_ACTION);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;
  cell.textLabel.text = title;

  cell.accessoryView =
      [[UIImageView alloc] initWithImage:DefaultSymbolTemplateWithPointSize(
                                             kCircleSymbol, kRadioButtonSize)];
  cell.accessoryView.tintColor = [UIColor colorNamed:kGrey500Color];

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  _selection = indexPath.row == 0 ? IOSParcelTrackingOptInStatus::kAlwaysTrack
                                  : IOSParcelTrackingOptInStatus::kAskToTrack;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  UIImage* icon;
  if (@available(iOS 16.0, *)) {
    icon = DefaultSymbolWithPointSize(kButtonProgrammableSymbol,
                                      kSymbolAccessoryPointSize);
  } else {
    icon = DefaultSymbolWithPointSize(kCircleCircleFillSymbol,
                                      kSymbolAccessoryPointSize);
  }
  cell.accessoryView = [[UIImageView alloc] initWithImage:icon];
  cell.accessoryView.tintColor = [UIColor colorNamed:kBlueColor];
  [self updateButtonForState:UIControlStateNormal];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultSymbolTemplateWithPointSize(
                        kCircleSymbol, kSymbolAccessoryPointSize)];
  cell.accessoryView.tintColor = [UIColor colorNamed:kGrey500Color];
}

#pragma mark - Private

// Creates the view with the "always track" and "ask to track" options.
- (UITableView*)createOptionsView {
  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStylePlain];
  _tableView.layer.cornerRadius = kTableViewCornerRadius;
  _tableView.estimatedRowHeight = kTableViewEstimatedRowHeight;
  _tableView.scrollEnabled = NO;
  _tableView.showsVerticalScrollIndicator = NO;
  _tableView.delegate = self;
  _tableView.userInteractionEnabled = YES;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.dataSource = self;
  _tableView.separatorInset = UIEdgeInsetsZero;
  [_tableView registerClass:TableViewTextCell.class
      forCellReuseIdentifier:@"cell"];

  [NSLayoutConstraint activateConstraints:@[
    [_tableView.heightAnchor
        constraintEqualToConstant:kTableViewEstimatedRowHeight * 2],
  ]];

  return _tableView;
}

// Updates the "Enable Tracking" button. The button should be disabled initially
// and only enabled after an option, either "always track" or "ask to track",
// has been selected by the user.
- (void)updateButtonForState:(UIControlState)state {
  UIButton* button = self.primaryActionButton;
  if (state == UIControlStateDisabled) {
    button.userInteractionEnabled = NO;
    [button setBackgroundColor:[UIColor colorNamed:kGrey200Color]];
    [button setTitleColor:[UIColor colorNamed:kGrey600Color]
                 forState:UIControlStateNormal];
  } else if (state == UIControlStateNormal) {
    button.userInteractionEnabled = YES;
    [button setBackgroundColor:[UIColor colorNamed:kBlueColor]];
    [button setTitleColor:[UIColor colorNamed:kBackgroundColor]
                 forState:UIControlStateNormal];
  }
}

@end
