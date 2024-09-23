// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/legacy_download_manager_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/download_manager_metric_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_constants.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_state_view.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/download/ui_bundled/features.h"
#import "ios/chrome/browser/download/ui_bundled/radial_progress_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Additional left margin for close button.
const CGFloat kCloseButtonLeftMargin = 17;

// The size of the shadow used for background resizable image.
const CGFloat kTopShadowHeight = 8;
const CGFloat kLeftRightShadowHeight = 16;

// Height of download or install drive controls row.
const CGFloat kRowHeight = 48;

// Default number of lines displayed for download label.
const NSInteger kNumberOfLines = 1;

// Additional margin for status label while Incognito warning is showing.
const CGFloat kIncognitoWarningMargin = 16;

// Returns formatted size string.
NSString* GetSizeString(long long size_in_bytes) {
  return [NSByteCountFormatter
      stringFromByteCount:size_in_bytes
               countStyle:NSByteCountFormatterCountStyleFile];
}

}  // namespace

@interface LegacyDownloadManagerViewController () {
  UIButton* _closeButton;
  UILabel* _statusLabel;
  UIButton* _actionButton;
  UIButton* _installDriveButton;
  UIImageView* _installDriveIcon;
  UILabel* _installDriveLabel;
  RadialProgressView* _progressView;

  NSString* _fileName;
  int64_t _countOfBytesReceived;
  int64_t _countOfBytesExpectedToReceive;
  float _progress;
  DownloadManagerState _state;
  BOOL _installDriveButtonVisible;
  BOOL _incognitoDownloadsWarningVisible;
  BOOL _addedConstraints;  // YES if NSLayoutConstraits were added.
}

// UIView that contains the state symbol displayed.
@property(nonatomic, strong) DownloadManagerStateView* stateSymbol;

// Background is a resizable image with edge shadows.
@property(nonatomic, readonly) UIImageView* background;

// Download Manager UI has 2 rows. First row is always visible and contains
// essential download controls: close button, action button and status label.
// Second row is hidden by default and constains Install Google Drive button.
// The second row is visible if `_installDriveButtonVisible` is set to YES.
// Each row is a UIView with controls as subviews, which allows to:
//   - vertically align all controls in the row
//   - hide all controls in a row altogether
//   - set proper constraits to size self.view
@property(nonatomic, readonly) UIView* downloadControlsRow;
@property(nonatomic, readonly) UIView* installDriveControlsRow;

// Grey line which separates downloadControlsRow and installDriveControlsRow.
@property(nonatomic, readonly) UIView* horizontalLine;

// Represents constraint for kBottomMarginGuide's topAnchor, which can either be
// constrained to installDriveControlsRow's bottomAnchor or to
// downloadControlsRow's bottomAnchor. Stored in a property to allow
// deactivating the old constraint.
@property(nonatomic) NSLayoutConstraint* bottomMarginGuideTopConstraint;

// Represents constraint for self.view.widthAnchor, which is anchored to
// superview with different multipliers depending on size class. Stored in a
// property to allow deactivating the old constraint.
@property(nonatomic) NSLayoutConstraint* viewWidthConstraint;

// Leading and trailing constraints for download and install drive controls.
@property(nonatomic) NSLayoutConstraint* downloadControlsRowLeadingConstraint;
@property(nonatomic) NSLayoutConstraint* downloadControlsRowTrailingConstraint;
@property(nonatomic) NSLayoutConstraint* downloadControlsRowHeightConstraint;
@property(nonatomic)
    NSLayoutConstraint* installDriveControlsRowLeadingConstraint;
@property(nonatomic)
    NSLayoutConstraint* installDriveControlsRowTrailingConstraint;

// Represents constraint for self.view.statusLabel, which is either anchored to
// self.closeButton or to self.actionButton (when visible).
@property(nonatomic) NSLayoutConstraint* statusLabelTrailingConstraint;

// UILayoutGuide for adding bottom margin to Download Manager view.
@property(nonatomic) UILayoutGuide* bottomMarginGuide;

@end

@implementation LegacyDownloadManagerViewController

@synthesize delegate = _delegate;
@synthesize background = _background;
@synthesize downloadControlsRow = _downloadControlsRow;
@synthesize installDriveControlsRow = _installDriveControlsRow;
@synthesize horizontalLine = _horizontalLine;
@synthesize bottomMarginGuideTopConstraint = _bottomMarginGuideTopConstraint;
@synthesize viewWidthConstraint = _viewWidthConstraint;
@synthesize downloadControlsRowLeadingConstraint =
    _downloadControlsRowLeadingConstraint;
@synthesize downloadControlsRowTrailingConstraint =
    _downloadControlsRowTrailingConstraint;
@synthesize downloadControlsRowHeightConstraint =
    _downloadControlsRowHeightConstraint;
@synthesize installDriveControlsRowLeadingConstraint =
    _installDriveControlsRowLeadingConstraint;
@synthesize installDriveControlsRowTrailingConstraint =
    _installDriveControlsRowTrailingConstraint;
@synthesize statusLabelTrailingConstraint = _statusLabelTrailingConstraint;
@synthesize bottomMarginGuide = _bottomMarginGuide;
@synthesize incognito = _incognito;

#pragma mark - UIViewController overrides

- (void)viewDidLoad {
  [super viewDidLoad];

  [self.view addSubview:self.background];
  [self.view addSubview:self.downloadControlsRow];
  [self.view addSubview:self.installDriveControlsRow];
  [self.downloadControlsRow addSubview:self.closeButton];
  [self.downloadControlsRow addSubview:self.stateSymbol];
  [self.downloadControlsRow addSubview:self.statusLabel];
  [self.downloadControlsRow addSubview:self.progressView];
  [self.downloadControlsRow addSubview:self.actionButton];
  [self.installDriveControlsRow addSubview:self.installDriveButton];
  [self.installDriveControlsRow addSubview:self.installDriveIcon];
  [self.installDriveControlsRow addSubview:self.installDriveLabel];
  [self.installDriveControlsRow addSubview:self.horizontalLine];

  self.bottomMarginGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:self.bottomMarginGuide];
}

- (void)updateViewConstraints {
  if (_addedConstraints) {
    [super updateViewConstraints];
    return;
  }

  // self.view constraints.
  UIView* view = self.view;
  UILayoutGuide* bottomMarginGuide = self.bottomMarginGuide;
  [NSLayoutConstraint activateConstraints:@[
    [view.bottomAnchor constraintEqualToAnchor:bottomMarginGuide.bottomAnchor],
  ]];
  [self updateBottomMarginGuideTopConstraint];

  // background constraints.
  UIView* background = self.background;
  [NSLayoutConstraint activateConstraints:@[
    [background.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [background.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [background.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [background.topAnchor constraintEqualToAnchor:view.topAnchor],
  ]];

  // download controls row constraints.
  UIView* downloadRow = self.downloadControlsRow;
  UIButton* closeButton = self.closeButton;
  self.downloadControlsRowLeadingConstraint =
      [downloadRow.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
  self.downloadControlsRowTrailingConstraint =
      [downloadRow.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
  [NSLayoutConstraint activateConstraints:@[
    self.downloadControlsRowLeadingConstraint,
    self.downloadControlsRowTrailingConstraint,
    [downloadRow.topAnchor constraintEqualToAnchor:view.topAnchor
                                          constant:kTopShadowHeight],
  ]];
  [self updateDownloadControlsRowHeightConstraint];

  // install drive controls row constraints.
  UIView* horizontalLine = self.horizontalLine;
  UIView* installDriveRow = self.installDriveControlsRow;
  UIButton* installDriveButton = self.installDriveButton;
  self.installDriveControlsRowLeadingConstraint = [installDriveRow.leadingAnchor
      constraintEqualToAnchor:view.leadingAnchor],
  self.installDriveControlsRowTrailingConstraint =
      [installDriveRow.trailingAnchor
          constraintEqualToAnchor:view.trailingAnchor],
  [NSLayoutConstraint activateConstraints:@[
    self.installDriveControlsRowLeadingConstraint,
    self.installDriveControlsRowTrailingConstraint,
    [installDriveRow.topAnchor
        constraintEqualToAnchor:horizontalLine.bottomAnchor],
    [installDriveRow.heightAnchor constraintEqualToConstant:kRowHeight],
  ]];

  // bottom margin row constraints.
  UILayoutGuide* secondaryToolbarGuide =
      [self.layoutGuideCenter makeLayoutGuideNamed:kSecondaryToolbarGuide];
  [self.view addLayoutGuide:secondaryToolbarGuide];
  [NSLayoutConstraint activateConstraints:@[
    [bottomMarginGuide.heightAnchor
        constraintEqualToAnchor:secondaryToolbarGuide.heightAnchor],
  ]];

  // close button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [closeButton.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:downloadRow.layoutMarginsGuide.trailingAnchor
                       constant:-4],
  ]];

  // state symbol constraints.
  UIView* stateSymbol = self.stateSymbol;
  [NSLayoutConstraint activateConstraints:@[
    [stateSymbol.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [stateSymbol.leadingAnchor
        constraintEqualToAnchor:downloadRow.layoutMarginsGuide.leadingAnchor
                       constant:3],
  ]];

  // progress view constraints.
  RadialProgressView* progressView = self.progressView;
  [NSLayoutConstraint activateConstraints:@[
    [progressView.leadingAnchor
        constraintEqualToAnchor:stateSymbol.leadingAnchor],
    [progressView.trailingAnchor
        constraintEqualToAnchor:stateSymbol.trailingAnchor],
    [progressView.topAnchor constraintEqualToAnchor:stateSymbol.topAnchor],
    [progressView.bottomAnchor
        constraintEqualToAnchor:stateSymbol.bottomAnchor],
  ]];

  // status label constraints.
  UILabel* statusLabel = self.statusLabel;
  UIButton* actionButton = self.actionButton;
  [NSLayoutConstraint activateConstraints:@[
    [statusLabel.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [statusLabel.leadingAnchor
        constraintEqualToAnchor:stateSymbol.trailingAnchor
                       constant:11],
  ]];
  [self updateStatusLabelTrailingConstraint];

  // action button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [actionButton.centerYAnchor
        constraintEqualToAnchor:downloadRow.centerYAnchor],
    [actionButton.trailingAnchor
        constraintEqualToAnchor:closeButton.leadingAnchor
                       constant:-kCloseButtonLeftMargin],
  ]];

  // install google drive button constraints.
  [NSLayoutConstraint activateConstraints:@[
    [installDriveButton.centerYAnchor
        constraintEqualToAnchor:installDriveRow.centerYAnchor],
    [installDriveButton.trailingAnchor
        constraintEqualToAnchor:actionButton.trailingAnchor],
  ]];

  // install google drive icon constraints.
  UIImageView* installDriveIcon = self.installDriveIcon;
  [NSLayoutConstraint activateConstraints:@[
    [installDriveIcon.centerYAnchor
        constraintEqualToAnchor:installDriveRow.centerYAnchor],
    [installDriveIcon.centerXAnchor
        constraintEqualToAnchor:stateSymbol.centerXAnchor],
  ]];

  // install google drive label constraints.
  UILabel* installDriveLabel = self.installDriveLabel;
  [NSLayoutConstraint activateConstraints:@[
    [installDriveLabel.centerYAnchor
        constraintEqualToAnchor:installDriveRow.centerYAnchor],
    [installDriveLabel.leadingAnchor
        constraintEqualToAnchor:statusLabel.leadingAnchor],
    [installDriveLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:installDriveButton.leadingAnchor
                                 constant:-kCloseButtonLeftMargin],
  ]];

  // constraint line which separates download controls and install drive rows.
  [NSLayoutConstraint activateConstraints:@[
    [horizontalLine.heightAnchor constraintEqualToConstant:1],
    [horizontalLine.topAnchor constraintEqualToAnchor:downloadRow.bottomAnchor],
    [horizontalLine.leadingAnchor
        constraintEqualToAnchor:installDriveRow.leadingAnchor],
    [horizontalLine.trailingAnchor
        constraintEqualToAnchor:installDriveRow.trailingAnchor],
  ]];

  [self updateConstraintsForTraitCollection:self.traitCollection];

  _addedConstraints = YES;
  [super updateViewConstraints];
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  auto block = ^(id<UIViewControllerTransitionCoordinatorContext> context) {
    [self updateConstraintsForTraitCollection:newCollection];
    [self updateBackgroundForTraitCollection:newCollection];
  };
  [coordinator animateAlongsideTransition:block completion:nil];
}

#pragma mark - Public

- (UIView*)openInSourceView {
  return self.actionButton;
}

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.overrideUserInterfaceStyle =
      incognito ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
}

- (void)setFileName:(NSString*)fileName {
  if (![_fileName isEqualToString:fileName]) {
    _fileName = [fileName copy];
    [self updateStatusLabel];
  }
}

- (void)setCountOfBytesReceived:(int64_t)value {
  if (_countOfBytesReceived != value) {
    _countOfBytesReceived = value;
    [self updateStatusLabel];
  }
}

- (void)setCountOfBytesExpectedToReceive:(int64_t)value {
  if (_countOfBytesExpectedToReceive != value) {
    _countOfBytesExpectedToReceive = value;
    [self updateStatusLabel];
  }
}

- (void)setProgress:(float)value {
  if (_progress != value) {
    _progress = value;
    [self updateProgressView];
  }
}

- (void)setState:(DownloadManagerState)state {
  if (_state != state) {
    _state = state;

    [self updateStateSymbol];
    [self updateStatusLabel];
    [self updateActionButton];
    [self updateProgressView];
    [self updateStatusLabelTrailingConstraint];
    [self updateDownloadControlsRowHeightConstraint];
  }
}

- (void)setInstallDriveButtonVisible:(BOOL)visible animated:(BOOL)animated {
  if (visible == _installDriveButtonVisible) {
    return;
  }

  _installDriveButtonVisible = visible;
  __weak LegacyDownloadManagerViewController* weakSelf = self;
  [UIView animateWithDuration:animated ? kDownloadManagerAnimationDuration : 0.0
                   animations:^{
                     LegacyDownloadManagerViewController* strongSelf = weakSelf;
                     [strongSelf updateInstallDriveControlsRow];
                     [strongSelf updateBottomMarginGuideTopConstraint];
                     [strongSelf.view.superview layoutIfNeeded];
                   }];
}

- (void)setFullscreenController:(FullscreenController*)fullscreenController {
  // Unsupported by this version of the DownloadManagerViewController.
  // This method will still be called, but do nothing.
}

#pragma mark - UI elements

- (UIImageView*)background {
  if (!_background) {
    _background = [[UIImageView alloc] initWithImage:nil];
    _background.translatesAutoresizingMaskIntoConstraints = NO;
    [self updateBackgroundForTraitCollection:self.traitCollection];
  }
  return _background;
}

- (UIView*)downloadControlsRow {
  if (!_downloadControlsRow) {
    _downloadControlsRow = [[UIView alloc] initWithFrame:CGRectZero];
    _downloadControlsRow.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _downloadControlsRow;
}

- (UIView*)installDriveControlsRow {
  if (!_installDriveControlsRow) {
    _installDriveControlsRow = [[UIView alloc] initWithFrame:CGRectZero];
    _installDriveControlsRow.translatesAutoresizingMaskIntoConstraints = NO;
    [self updateInstallDriveControlsRow];
  }
  return _installDriveControlsRow;
}

- (UIButton*)closeButton {
  if (!_closeButton) {
    _closeButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    _closeButton.exclusiveTouch = YES;
    _closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);

    UIImage* image = DefaultSymbolTemplateWithPointSize(
        kXMarkSymbol, kSymbolDownloadInfobarPointSize);
    [_closeButton setImage:image forState:UIControlStateNormal];
    _closeButton.tintColor = [UIColor colorNamed:kToolbarButtonColor];

    [_closeButton addTarget:self
                     action:@selector(didTapCloseButton)
           forControlEvents:UIControlEventTouchUpInside];

    _closeButton.pointerInteractionEnabled = YES;
    _closeButton.accessibilityIdentifier =
        kDownloadManagerCloseButtonAccessibilityIdentifier;
  }
  return _closeButton;
}

- (DownloadManagerStateView*)stateSymbol {
  if (!_stateSymbol) {
    _stateSymbol = [[DownloadManagerStateView alloc] init];
    _stateSymbol.translatesAutoresizingMaskIntoConstraints = NO;
    _stateSymbol.contentMode = UIViewContentModeCenter;

    [self updateStateSymbol];
  }
  return _stateSymbol;
}

- (UILabel*)statusLabel {
  if (!_statusLabel) {
    _statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    const CGFloat kBody1FontSize = 14.0;
    const UIFontWeight kBody1FontWeight = UIFontWeightRegular;
    _statusLabel.font = [UIFont systemFontOfSize:kBody1FontSize
                                          weight:kBody1FontWeight];
    _statusLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    [_statusLabel
        setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self updateStatusLabel];
  }
  return _statusLabel;
}

- (UIButton*)actionButton {
  if (!_actionButton) {
    _actionButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _actionButton.translatesAutoresizingMaskIntoConstraints = NO;
    _actionButton.exclusiveTouch = YES;
    const CGFloat kButtonFontSize = 14.0;
    const UIFontWeight kButtonFontWeight = UIFontWeightMedium;
    _actionButton.titleLabel.font = [UIFont systemFontOfSize:kButtonFontSize
                                                      weight:kButtonFontWeight];
    [_actionButton setTitleColor:[UIColor colorNamed:kBlueColor]
                        forState:UIControlStateNormal];

    [_actionButton addTarget:self
                      action:@selector(didTapActionButton)
            forControlEvents:UIControlEventTouchUpInside];

    _actionButton.pointerInteractionEnabled = YES;

    [self updateActionButton];
  }
  return _actionButton;
}

- (UIButton*)installDriveButton {
  if (!_installDriveButton) {
    _installDriveButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _installDriveButton.translatesAutoresizingMaskIntoConstraints = NO;
    _installDriveButton.exclusiveTouch = YES;
    const CGFloat kButtonFontSize = 14.0;
    const UIFontWeight kButtonFontWeight = UIFontWeightMedium;
    _installDriveButton.titleLabel.font =
        [UIFont systemFontOfSize:kButtonFontSize weight:kButtonFontWeight];
    [_installDriveButton setTitleColor:[UIColor colorNamed:kBlueColor]
                              forState:UIControlStateNormal];

    [_installDriveButton addTarget:self
                            action:@selector(didTapInstallDriveButton)
                  forControlEvents:UIControlEventTouchUpInside];
    [_installDriveButton
        setTitle:l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_INSTALL)
        forState:UIControlStateNormal];

    _installDriveButton.pointerInteractionEnabled = YES;
  }
  return _installDriveButton;
}

- (UIImageView*)installDriveIcon {
  if (!_installDriveIcon) {
    _installDriveIcon = [[UIImageView alloc] initWithFrame:CGRectZero];
    _installDriveIcon.translatesAutoresizingMaskIntoConstraints = NO;
    _installDriveIcon.image = ios::provider::GetBrandedImage(
        ios::provider::BrandedImage::kDownloadGoogleDrive);
  }
  return _installDriveIcon;
}

- (UILabel*)installDriveLabel {
  if (!_installDriveLabel) {
    _installDriveLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _installDriveLabel.translatesAutoresizingMaskIntoConstraints = NO;
    const CGFloat kBody1FontSize = 14.0;
    const UIFontWeight kBody1FontWeight = UIFontWeightRegular;
    _installDriveLabel.font = [UIFont systemFontOfSize:kBody1FontSize
                                                weight:kBody1FontWeight];
    _installDriveLabel.text =
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_GOOGLE_DRIVE);
    [_installDriveLabel sizeToFit];
  }
  return _installDriveLabel;
}

- (RadialProgressView*)progressView {
  if (!_progressView) {
    _progressView = [[RadialProgressView alloc] initWithFrame:CGRectZero];
    _progressView.translatesAutoresizingMaskIntoConstraints = NO;
    _progressView.lineWidth = 2;
    _progressView.progressTintColor = [UIColor colorNamed:kBlueColor];
    _progressView.trackTintColor = [UIColor colorNamed:kBlueHaloColor];
    [self updateProgressView];
  }
  return _progressView;
}

- (UIView*)horizontalLine {
  if (!_horizontalLine) {
    _horizontalLine = [[UIView alloc] init];
    _horizontalLine.translatesAutoresizingMaskIntoConstraints = NO;
    _horizontalLine.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  }
  return _horizontalLine;
}

#pragma mark - Actions

- (void)didTapCloseButton {
  SEL selector = @selector(downloadManagerViewControllerDidClose:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate downloadManagerViewControllerDidClose:self];
  }
}

- (void)didTapActionButton {
  switch (_state) {
    case kDownloadManagerStateNotStarted: {
      SEL selector = @selector(downloadManagerViewControllerDidStartDownload:);
      if ([_delegate respondsToSelector:selector]) {
        [_delegate downloadManagerViewControllerDidStartDownload:self];
      }
      break;
    }
    case kDownloadManagerStateInProgress: {
      // The button should not be visible.
      NOTREACHED_IN_MIGRATION();
      break;
    }
    case kDownloadManagerStateSucceeded: {
      SEL selector = @selector(presentOpenInForDownloadManagerViewController:);
      if ([_delegate respondsToSelector:selector]) {
        [_delegate presentOpenInForDownloadManagerViewController:self];
      }
      break;
    }
    case kDownloadManagerStateFailed: {
      SEL selector = @selector(downloadManagerViewControllerDidStartDownload:);
      if ([_delegate respondsToSelector:selector]) {
        [_delegate downloadManagerViewControllerDidStartDownload:self];
      }
      break;
    }
    case kDownloadManagerStateFailedNotResumable:
      // The button should not be visible
      break;
  }
}

- (void)didTapInstallDriveButton {
  base::UmaHistogramEnumeration(
      "Download.IOSDownloadFileUIGoogleDrive",
      DownloadFileUIGoogleDrive::GoogleDriveInstallStarted,
      DownloadFileUIGoogleDrive::Count);
  SEL selector = @selector(installDriveForDownloadManagerViewController:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate installDriveForDownloadManagerViewController:self];
  }
}

#pragma mark - UI Updates

// Updates and activates self.bottomMarginGuideTopConstraint.
// self.bottomMarginGuideTopConstraint constraints kBottomMarginGuide's
// topAnchor to installDriveControlsRow's bottom if `_installDriveButtonVisible`
// is set to YES, otherwise self.view.bottomAnchor is constrained to
// downloadControlsRow's bottom. This resizes self.view to show or hide
// installDriveControlsRow view.
- (void)updateBottomMarginGuideTopConstraint {
  if (!self.viewLoaded) {
    // This method will be called again when the view is loaded.
    return;
  }
  self.bottomMarginGuideTopConstraint.active = NO;
  NSLayoutYAxisAnchor* secondAnchor =
      _installDriveButtonVisible ? self.installDriveControlsRow.bottomAnchor
                                 : self.downloadControlsRow.bottomAnchor;

  self.bottomMarginGuideTopConstraint =
      [self.bottomMarginGuide.topAnchor constraintEqualToAnchor:secondAnchor];

  self.bottomMarginGuideTopConstraint.active = YES;
}

// Updates background image for the given UITraitCollection.
- (void)updateBackgroundForTraitCollection:(UITraitCollection*)traitCollection {
  NSString* imageName =
      traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular
          ? @"background_regular"
          : @"background_compact";

  self.background.image = [UIImage imageNamed:imageName];
}

// Updates and activates constraints which depend on ui size class.
- (void)updateConstraintsForTraitCollection:
    (UITraitCollection*)traitCollection {
  self.viewWidthConstraint.active = NO;

  // With regular horizontal size class, UI is too wide to take the full width,
  // because there will be a lot of blank space.
  BOOL regularSizeClass =
      traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
  self.viewWidthConstraint = [self.view.widthAnchor
      constraintEqualToAnchor:self.view.superview.widthAnchor
                   multiplier:regularSizeClass ? 0.6 : 1.0];

  self.viewWidthConstraint.active = YES;

  CGFloat constant = regularSizeClass ? kLeftRightShadowHeight / 2 : 0;
  self.downloadControlsRowLeadingConstraint.constant = constant;
  self.downloadControlsRowTrailingConstraint.constant = -constant;
  self.installDriveControlsRowLeadingConstraint.constant = constant;
  self.installDriveControlsRowTrailingConstraint.constant = -constant;
}

// Anchors self.view.statusLabel to self.closeButton or to self.actionButton
// (when download is not in progress and action button is visible).
- (void)updateStatusLabelTrailingConstraint {
  if (!self.viewLoaded || !self.view.superview) {
    // Constraints can not be set if UI elements do not have a common view.
    // This method will be called again when self.view is added to superview.
    return;
  }

  self.statusLabelTrailingConstraint.active = NO;

  UIView* secondAnchorElement = _state == kDownloadManagerStateInProgress
                                    ? self.closeButton
                                    : self.actionButton;

  self.statusLabelTrailingConstraint = [self.statusLabel.trailingAnchor
      constraintLessThanOrEqualToAnchor:secondAnchorElement.leadingAnchor
                               constant:-kCloseButtonLeftMargin];

  self.statusLabelTrailingConstraint.active = YES;
}

// Updates the height anchor for self.view.downloadControlsRow to fit the text
// when Incognito warning is visible and resets the height anchor when the
// Incognito warning is no longer visible.
- (void)updateDownloadControlsRowHeightConstraint {
  if (!self.viewLoaded || !self.view.superview) {
    // Constraints can not be set if UI elements do not have a common view.
    // This method will be called again when self.view is added to superview.
    return;
  }

  self.downloadControlsRowHeightConstraint.active = NO;

  if (_incognitoDownloadsWarningVisible) {
    self.downloadControlsRowHeightConstraint =
        [self.downloadControlsRow.heightAnchor
            constraintEqualToAnchor:self.statusLabel.heightAnchor
                           constant:kIncognitoWarningMargin];
  } else {
    self.downloadControlsRowHeightConstraint =
        [self.downloadControlsRow.heightAnchor
            constraintEqualToConstant:kRowHeight];
  }

  self.downloadControlsRowHeightConstraint.active = YES;
}

// Updates state symbol depending on the current download state.
- (void)updateStateSymbol {
  [self.stateSymbol setState:_state];
}

// Updates status label text depending on `state`.
- (void)updateStatusLabel {
  NSString* statusText = nil;
  _incognitoDownloadsWarningVisible = NO;
  self.statusLabel.numberOfLines = kNumberOfLines;
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      if (self.incognito) {
        _incognitoDownloadsWarningVisible = YES;
        statusText =
            l10n_util::GetNSString(IDS_IOS_DOWNLOAD_INCOGNITO_WARNING_MESSAGE);
        // Set to '0' to ensure the entire incognito warning is visible.
        self.statusLabel.numberOfLines = 0;
        break;
      }

      statusText = _fileName;
      if (_countOfBytesExpectedToReceive != -1) {
        statusText = [statusText
            stringByAppendingFormat:@" - %@",
                                    GetSizeString(
                                        _countOfBytesExpectedToReceive)];
      }
      break;
    case kDownloadManagerStateInProgress: {
      std::u16string size =
          base::SysNSStringToUTF16(GetSizeString(_countOfBytesReceived));
      statusText = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_DOWNLOADING_ELIPSIS, size);
      if (_countOfBytesExpectedToReceive != -1) {
        statusText = [statusText
            stringByAppendingFormat:@"/%@",
                                    GetSizeString(
                                        _countOfBytesExpectedToReceive)];
      }
      break;
    }
    case kDownloadManagerStateSucceeded:
      statusText = _fileName;
      break;
    case kDownloadManagerStateFailed:
      statusText =
          l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_COULDNT_DOWNLOAD);
      break;
    case kDownloadManagerStateFailedNotResumable:
      statusText =
          l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_CANNOT_BE_RETRIED);
      break;
  }

  self.statusLabel.text = statusText;
}

// Updates title and hidden state for action button depending on `state`.
- (void)updateActionButton {
  NSString* title = nil;
  NSString* accessibilityIdentifier = nil;
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD);
      accessibilityIdentifier = kDownloadManagerDownloadAccessibilityIdentifier;
      break;
    case kDownloadManagerStateInProgress:
      break;
    case kDownloadManagerStateSucceeded:
      title = l10n_util::GetNSString(IDS_IOS_OPEN_IN);
      accessibilityIdentifier = kDownloadManagerOpenInAccessibilityIdentifier;
      break;
    case kDownloadManagerStateFailed:
      title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_TRY_AGAIN);
      accessibilityIdentifier = kDownloadManagerTryAgainAccessibilityIdentifier;
      break;
    case kDownloadManagerStateFailedNotResumable:
      break;
  }

  [self.actionButton setTitle:title forState:UIControlStateNormal];
  [self.actionButton setAccessibilityIdentifier:accessibilityIdentifier];
  self.actionButton.hidden =
      (_state == kDownloadManagerStateInProgress ||
       _state == kDownloadManagerStateFailedNotResumable);
}

- (void)updateProgressView {
  self.progressView.hidden = _state != kDownloadManagerStateInProgress;
  self.progressView.progress = _progress;
}

// Updates alpha value for install google drive controls row.
// Makes whole installDriveControlsRow opaque if
// _installDriveButtonVisible is set to YES, otherwise makes the row
// fully transparent.
- (void)updateInstallDriveControlsRow {
  self.installDriveControlsRow.alpha = _installDriveButtonVisible ? 1.0f : 0.0f;
}

@end
