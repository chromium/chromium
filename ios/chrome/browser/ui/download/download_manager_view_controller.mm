// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/download/download_manager_constants.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/ui/download/features.h"
#import "ios/chrome/browser/ui/download/radial_progress_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
// Names of icons used in Download buttons or as leading icon.
NSString* const kFilesAppImage = @"apple_files_app";
NSString* const kFilesAppWithBackgroundImage =
    @"apple_files_app_with_background";
NSString* const kDriveAppImage = @"google_drive_app";
NSString* const kDriveAppWithBackgroundImage =
    @"google_drive_app_with_background";
#endif

// `self.view` constants.
constexpr CGFloat kWidthConstraintRegularMultiplier = 0.6;
constexpr CGFloat kWidthConstraintCompactMultiplier = 1.0;

// Download controls row constants.
constexpr CGFloat kRowHeight = 32;
constexpr CGFloat kRowHorizontalMargins = 16;
constexpr CGFloat kRowVerticalMargins = 8;
constexpr CGFloat kRowSpacing = 8;

// Other UI elements constants.
constexpr CGFloat kLeadingIconSize = 24;
constexpr CGFloat kLeadingIconBorderWidth = 1;
constexpr CGFloat kLeadingIconCornerRadius = 3.5;
constexpr CGFloat kTextStackSpacing = 2;
constexpr CGFloat kDownloadButtonHorizontalInset = 8;
constexpr CGFloat kDownloadButtonVerticalInset = 4;
constexpr CGFloat kDownloadButtonImagePadding = 4;
constexpr CGFloat kProgressViewLineWidth = 2.5;
constexpr CGFloat kCloseButtonIconSize = 30;

// Returns formatted size string.
NSString* GetSizeString(int64_t size_in_bytes) {
  NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
  formatter.countStyle = NSByteCountFormatterCountStyleFile;
  formatter.zeroPadsFractionDigits = YES;
  NSString* result = [formatter stringFromByteCount:size_in_bytes];
  // Replace spaces with non-breaking spaces.
  result = [result stringByReplacingOccurrencesOfString:@" "
                                             withString:@"\u00A0"];
  return result;
}

// Returns the appropriate image for a destination icon, with or without
// background.
UIImage* GetDownloadFileDestinationImage(DownloadFileDestination destination,
                                         bool with_background) {
  NSString* image_name = nil;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  switch (destination) {
    case DownloadFileDestination::kFiles:
      image_name =
          with_background ? kFilesAppWithBackgroundImage : kFilesAppImage;
      break;
    case DownloadFileDestination::kDrive:
      image_name =
          with_background ? kDriveAppWithBackgroundImage : kDriveAppImage;
      break;
  }

#endif
  return image_name ? [UIImage imageNamed:image_name] : nil;
}

// Creates a button configuration for a download button.
UIButtonConfiguration* CreateDownloadButtonConfiguration(
    NSString* title,
    DownloadFileDestination destination,
    bool use_image,
    bool use_image_background) {
  UIButtonConfiguration* conf = [UIButtonConfiguration grayButtonConfiguration];
  conf.contentInsets = NSDirectionalEdgeInsetsMake(
      kDownloadButtonVerticalInset, kDownloadButtonHorizontalInset,
      kDownloadButtonVerticalInset, kDownloadButtonHorizontalInset);
  conf.imagePlacement = NSDirectionalRectEdgeTop;
  conf.imagePadding = kDownloadButtonImagePadding;
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  if (use_image) {
    conf.image =
        GetDownloadFileDestinationImage(destination, use_image_background);
  }
#endif
  if (title) {
    NSMutableParagraphStyle* centered_style =
        [[NSMutableParagraphStyle alloc] init];
    centered_style.alignment = NSTextAlignmentCenter;
    conf.attributedTitle = [[NSAttributedString alloc]
        initWithString:title
            attributes:@{NSParagraphStyleAttributeName : centered_style}];
  }
  return conf;
}

// Creates a button for an action button ("OPEN", "GET THE APP" or "TRY AGAIN")
UIButton* CreateActionButton(NSString* title,
                             NSString* accessibility_identifier,
                             UIAction* action) {
  UIButtonConfiguration* conf =
      [UIButtonConfiguration plainButtonConfiguration];
  conf.buttonSize = UIButtonConfigurationSizeSmall;
  conf.title = title;
  UIButton* button = [UIButton buttonWithConfiguration:conf
                                         primaryAction:action];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];
  [button
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
  button.accessibilityIdentifier = accessibility_identifier;
  return button;
}

// Creates an icon to be added in the center of the radial progress view.
UIImageView* CreateProgressIcon(NSString* symbol_name) {
  UIImageConfiguration* image_configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolDownloadInfobarPointSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleSmall];
  UIImage* image;
  image = DefaultSymbolWithConfiguration(symbol_name, image_configuration);
  UIImageView* icon = [[UIImageView alloc] initWithImage:image];
  icon.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  return icon;
}

}  // namespace

@interface DownloadManagerViewController () {
  NSString* _fileName;
  int64_t _countOfBytesReceived;
  int64_t _countOfBytesExpectedToReceive;
  float _progress;
  DownloadManagerState _state;
  BOOL _installDriveButtonVisible;
  BOOL _downloadToDriveButtonVisible;
  DownloadFileDestination _downloadFileDestination;
  NSString* _saveToDriveUserEmail;
  BOOL _addedConstraints;  // YES if NSLayoutConstraits were added.

  // UI elements.
  UIImageView* _leadingIcon;
  UILabel* _statusLabel;
  UILabel* _detailLabel;
  UIStackView* _textStack;
  UIButton* _downloadToFilesButton;
  UIButton* _downloadToDriveButton;
  RadialProgressView* _progressView;
  UIImageView* _filesProgressIcon;
  UIImageView* _driveProgressIcon;
  UIButton* _openInButton;
  UIButton* _openInDriveButton;
  UIButton* _installAppButton;
  UIButton* _tryAgainButton;
  UIButton* _closeButton;
  UIStackView* _downloadControlsRow;
}

@property(nonatomic, readonly) UIImageView* leadingIcon;
@property(nonatomic, readonly) UILabel* statusLabel;
@property(nonatomic, readonly) UILabel* detailLabel;
@property(nonatomic, readonly) UIStackView* textStack;
@property(nonatomic, readonly) UIButton* downloadToFilesButton;
@property(nonatomic, readonly) UIButton* downloadToDriveButton;
@property(nonatomic, readonly) RadialProgressView* progressView;
@property(nonatomic, readonly) UIImageView* filesProgressIcon;
@property(nonatomic, readonly) UIButton* openInButton;
@property(nonatomic, readonly) UIButton* tryAgainButton;
@property(nonatomic, readonly) UIButton* closeButton;
@property(nonatomic, readonly) UIStackView* downloadControlsRow;

// Represents constraint for self.view.widthAnchor, which is anchored to
// superview with different multipliers depending on size class. Stored in a
// property to allow deactivating the old constraint.
@property(strong, nonatomic) NSLayoutConstraint* viewWidthConstraint;

// UILayoutGuide for adding bottom margin to Download Manager view.
@property(strong, nonatomic) UILayoutGuide* bottomMarginGuide;

@end

@implementation DownloadManagerViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Set `self.view` properties (background, shadow, etc).
  self.view.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityMedium;
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view.layer.shadowColor =
      [UIColor colorNamed:kToolbarShadowColor].CGColor;
  self.view.layer.shadowOpacity = 1.0;
  self.view.layer.shadowOffset = CGSizeZero;

  // Create hierarchy of subviews.
  [self.downloadControlsRow addArrangedSubview:self.leadingIcon];
  [self.textStack addArrangedSubview:self.statusLabel];
  [self.textStack addArrangedSubview:self.detailLabel];
  [self.downloadControlsRow addArrangedSubview:self.textStack];
  [self.downloadControlsRow addArrangedSubview:self.downloadToFilesButton];
  [self.downloadControlsRow addArrangedSubview:self.downloadToDriveButton];
  [self.progressView addSubview:self.filesProgressIcon];
  [self.progressView addSubview:self.driveProgressIcon];
  [self.downloadControlsRow addArrangedSubview:self.progressView];
  [self.downloadControlsRow addArrangedSubview:self.openInButton];
  [self.downloadControlsRow addArrangedSubview:self.openInDriveButton];
  [self.downloadControlsRow addArrangedSubview:self.installAppButton];
  [self.downloadControlsRow addArrangedSubview:self.tryAgainButton];
  [self.downloadControlsRow addArrangedSubview:self.closeButton];
  [self.view addSubview:self.downloadControlsRow];

  self.bottomMarginGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:self.bottomMarginGuide];
}

- (void)updateViewConstraints {
  if (_addedConstraints) {
    [super updateViewConstraints];
    return;
  }

  // `self.view`, bottom margin, download controls row constraints.
  UIView* view = self.view;
  UILayoutGuide* bottomMarginGuide = self.bottomMarginGuide;
  UIView* downloadRow = self.downloadControlsRow;
  UILayoutGuide* secondaryToolbarGuide =
      [self.layoutGuideCenter makeLayoutGuideNamed:kSecondaryToolbarGuide];
  [view addLayoutGuide:secondaryToolbarGuide];

  [NSLayoutConstraint activateConstraints:@[
    [bottomMarginGuide.bottomAnchor constraintEqualToAnchor:view.bottomAnchor],
    [bottomMarginGuide.heightAnchor
        constraintGreaterThanOrEqualToAnchor:secondaryToolbarGuide
                                                 .heightAnchor],
    [bottomMarginGuide.topAnchor
        constraintLessThanOrEqualToAnchor:view.safeAreaLayoutGuide
                                              .bottomAnchor],
    [downloadRow.bottomAnchor
        constraintEqualToAnchor:bottomMarginGuide.topAnchor
                       constant:-kRowVerticalMargins],
    [downloadRow.topAnchor constraintEqualToAnchor:view.topAnchor
                                          constant:kRowVerticalMargins],
    [downloadRow.centerXAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.centerXAnchor],
    [downloadRow.widthAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.widthAnchor
                       constant:-2 * kRowHorizontalMargins],
    [downloadRow.heightAnchor
        constraintGreaterThanOrEqualToConstant:kRowHeight],
  ]];

  // Leading icon contraints.
  UIView* leadingIcon = self.leadingIcon;
  [NSLayoutConstraint activateConstraints:@[
    [leadingIcon.widthAnchor constraintEqualToConstant:kLeadingIconSize],
    [leadingIcon.heightAnchor constraintEqualToConstant:kLeadingIconSize],
  ]];

  // Progress view constraints.
  UIView* progressView = self.progressView;
  [NSLayoutConstraint activateConstraints:@[
    [progressView.widthAnchor
        constraintEqualToAnchor:self.closeButton.widthAnchor],
    [progressView.heightAnchor
        constraintEqualToAnchor:progressView.widthAnchor],
  ]];
  AddSameCenterConstraints(self.filesProgressIcon, progressView);
  AddSameCenterConstraints(self.driveProgressIcon, progressView);

  // Download buttons constraints.
  UIView* downloadToFilesButton = self.downloadToFilesButton;
  UIView* downloadToDriveButton = self.downloadToDriveButton;
  [NSLayoutConstraint activateConstraints:@[
    [downloadToFilesButton.heightAnchor
        constraintEqualToAnchor:downloadToDriveButton.heightAnchor],
  ]];

  [self updateConstraintsForTraitCollection:self.traitCollection];

  _addedConstraints = YES;
  [super updateViewConstraints];
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  __weak __typeof(self) weakSelf = self;
  auto transition =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateConstraintsForTraitCollection:newCollection];
      };
  auto completion =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateViews];
      };
  [coordinator animateAlongsideTransition:transition completion:completion];
}

#pragma mark - DownloadManagerConsumer

- (void)setIncognito:(BOOL)incognito {
  _incognito = incognito;
  self.overrideUserInterfaceStyle =
      incognito && base::FeatureList::IsEnabled(kIOSIncognitoDownloadsWarning)
          ? UIUserInterfaceStyleDark
          : UIUserInterfaceStyleUnspecified;
}

- (void)setFileName:(NSString*)fileName {
  if (![_fileName isEqualToString:fileName]) {
    _fileName = [fileName copy];
    [self updateViews];
  }
}

- (void)setCountOfBytesReceived:(int64_t)value {
  if (_countOfBytesReceived != value) {
    _countOfBytesReceived = value;
    [self updateViews];
  }
}

- (void)setCountOfBytesExpectedToReceive:(int64_t)value {
  if (_countOfBytesExpectedToReceive != value) {
    _countOfBytesExpectedToReceive = value;
    [self updateViews];
  }
}

- (void)setProgress:(float)value {
  if (_progress != value) {
    _progress = value;
    [self updateViews];
  }
}

- (void)setState:(DownloadManagerState)state {
  if (_state != state) {
    _state = state;
    [self updateViews];
  }
}

- (void)setInstallDriveButtonVisible:(BOOL)visible animated:(BOOL)animated {
  if (_installDriveButtonVisible != visible) {
    _installDriveButtonVisible = visible;
    [self updateViews];
  }
}

- (void)setDownloadToDriveButtonVisible:(BOOL)visible {
  if (_downloadToDriveButtonVisible != visible) {
    _downloadToDriveButtonVisible = visible;
    [self updateViews];
  }
}

- (void)setDownloadFileDestination:(DownloadFileDestination)destination {
  if (_downloadFileDestination != destination) {
    _downloadFileDestination = destination;
    [self updateViews];
  }
}

- (void)setSaveToDriveUserEmail:(NSString*)userEmail {
  if (![userEmail isEqualToString:_saveToDriveUserEmail]) {
    _saveToDriveUserEmail = userEmail;
    [self updateViews];
  }
}

#pragma mark - DownloadManagerViewControllerProtocol

- (UIView*)openInSourceView {
  return nil;
}

#pragma mark - UI elements

- (UIImageView*)leadingIcon {
  if (!_leadingIcon) {
    _leadingIcon = [[UIImageView alloc] init];
    _leadingIcon.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingIcon.contentMode = UIViewContentModeCenter;
    _leadingIcon.layer.borderColor =
        [[UIColor colorNamed:kGrey200Color]
            resolvedColorWithTraitCollection:
                [UITraitCollection traitCollectionWithUserInterfaceStyle:
                                       UIUserInterfaceStyleLight]]
            .CGColor;
    _leadingIcon.layer.borderWidth = kLeadingIconBorderWidth;
    _leadingIcon.layer.cornerRadius = kLeadingIconCornerRadius;
    [_leadingIcon setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
  }

  return _leadingIcon;
}

- (UILabel*)statusLabel {
  if (!_statusLabel) {
    _statusLabel = [[UILabel alloc] init];
    _statusLabel.adjustsFontForContentSizeCategory = YES;
    _statusLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _statusLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _statusLabel.numberOfLines = 0;
  }

  return _statusLabel;
}

- (UILabel*)detailLabel {
  if (!_detailLabel) {
    _detailLabel = [[UILabel alloc] init];
    _detailLabel.adjustsFontForContentSizeCategory = YES;
    _detailLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _detailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }

  return _detailLabel;
}

- (UIStackView*)textStack {
  if (!_textStack) {
    _textStack = [[UIStackView alloc] init];
    _textStack.translatesAutoresizingMaskIntoConstraints = NO;
    _textStack.axis = UILayoutConstraintAxisVertical;
    _textStack.distribution = UIStackViewDistributionEqualCentering;
    _textStack.spacing = kTextStackSpacing;
    _textStack.alignment = UIStackViewAlignmentFill;
    [_textStack
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_textStack setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisVertical];
  }

  return _textStack;
}

- (UIButton*)downloadToFilesButton {
  if (!_downloadToFilesButton) {
    UIButtonConfiguration* downloadToFilesButtonConf =
        CreateDownloadButtonConfiguration(nil, DownloadFileDestination::kFiles,
                                          false, false);
    __weak __typeof(self) weakSelf = self;
    UIAction* downloadToFilesAction =
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              downloadManagerViewControllerDidStartDownload:weakSelf];
        }];
    _downloadToFilesButton =
        [UIButton buttonWithConfiguration:downloadToFilesButtonConf
                            primaryAction:downloadToFilesAction];
    [_downloadToFilesButton
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    [_downloadToFilesButton
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _downloadToFilesButton.accessibilityIdentifier =
        kDownloadManagerDownloadToFilesAccessibilityIdentifier;
  }

  return _downloadToFilesButton;
}

- (UIButton*)downloadToDriveButton {
  if (!_downloadToDriveButton) {
    UIButtonConfiguration* downloadToDriveButtonConf =
        CreateDownloadButtonConfiguration(nil, DownloadFileDestination::kDrive,
                                          false, false);
    __weak __typeof(self) weakSelf = self;
    UIAction* downloadToDriveAction =
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              downloadManagerViewControllerDidStartDownloadToDrive:weakSelf];
        }];
    _downloadToDriveButton =
        [UIButton buttonWithConfiguration:downloadToDriveButtonConf
                            primaryAction:downloadToDriveAction];
    [_downloadToDriveButton
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    [_downloadToDriveButton
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    _downloadToDriveButton.accessibilityIdentifier =
        kDownloadManagerDownloadToDriveAccessibilityIdentifier;
  }

  return _downloadToDriveButton;
}

- (RadialProgressView*)progressView {
  if (!_progressView) {
    _progressView = [[RadialProgressView alloc] init];
    _progressView.translatesAutoresizingMaskIntoConstraints = NO;
    _progressView.lineWidth = kProgressViewLineWidth;
    _progressView.progressTintColor = [UIColor colorNamed:kBlueColor];
    _progressView.trackTintColor = [UIColor colorNamed:kTextQuaternaryColor];
  }

  return _progressView;
}

- (UIImageView*)filesProgressIcon {
  if (!_filesProgressIcon) {
    _filesProgressIcon = CreateProgressIcon(kArrowDownSymbol);
  }

  return _filesProgressIcon;
}

- (UIImageView*)driveProgressIcon {
  if (!_driveProgressIcon) {
    _driveProgressIcon = CreateProgressIcon(kArrowUpSymbol);
  }

  return _driveProgressIcon;
}

- (UIButton*)openInButton {
  if (!_openInButton) {
    __weak __typeof(self) weakSelf = self;
    _openInButton = CreateActionButton(
        [l10n_util::GetNSString(IDS_IOS_OPEN_IN) localizedUppercaseString],
        kDownloadManagerOpenInAccessibilityIdentifier,
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              presentOpenInForDownloadManagerViewController:weakSelf];
        }]);
  }

  return _openInButton;
}

- (UIButton*)openInDriveButton {
  if (!_openInDriveButton) {
    __weak __typeof(self) weakSelf = self;
    _openInDriveButton = CreateActionButton(
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_OPEN),
        kDownloadManagerOpenInDriveAccessibilityIdentifier,
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              downloadManagerViewControllerDidOpenInDriveApp:weakSelf];
        }]);
  }

  return _openInDriveButton;
}

- (UIButton*)installAppButton {
  if (!_installAppButton) {
    __weak __typeof(self) weakSelf = self;
    _installAppButton = CreateActionButton(
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_GET_THE_APP),
        kDownloadManagerInstallAppAccessibilityIdentifier,
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              installDriveForDownloadManagerViewController:weakSelf];
        }]);
  }

  return _installAppButton;
}

- (UIButton*)tryAgainButton {
  if (!_tryAgainButton) {
    __weak __typeof(self) weakSelf = self;
    _tryAgainButton = CreateActionButton(
        [l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_TRY_AGAIN)
            localizedUppercaseString],
        kDownloadManagerTryAgainAccessibilityIdentifier,
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate downloadManagerViewControllerDidRetry:weakSelf];
        }]);
  }

  return _tryAgainButton;
}

- (UIButton*)closeButton {
  if (!_closeButton) {
    UIImage* closeButtonImage =
        SymbolWithPalette(DefaultSymbolWithPointSize(kXMarkCircleFillSymbol,
                                                     kCloseButtonIconSize),
                          @[
                            [UIColor colorNamed:kGrey600Color],
                            [UIColor colorNamed:kGrey200Color],
                          ]);
    UIButtonConfiguration* closeButtonConf =
        [UIButtonConfiguration plainButtonConfiguration];
    closeButtonConf.image = closeButtonImage;
    closeButtonConf.contentInsets = NSDirectionalEdgeInsetsZero;
    closeButtonConf.buttonSize = UIButtonConfigurationSizeSmall;
    closeButtonConf.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);
    __weak __typeof(self) weakSelf = self;
    UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.delegate downloadManagerViewControllerDidClose:weakSelf];
    }];
    _closeButton = [UIButton buttonWithConfiguration:closeButtonConf
                                       primaryAction:closeAction];
    [_closeButton setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
    [_closeButton
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
  }

  return _closeButton;
}

- (UIView*)downloadControlsRow {
  if (!_downloadControlsRow) {
    _downloadControlsRow = [[UIStackView alloc] initWithFrame:CGRectZero];
    _downloadControlsRow.translatesAutoresizingMaskIntoConstraints = NO;
    _downloadControlsRow.alignment = UIStackViewAlignmentCenter;
    _downloadControlsRow.axis = UILayoutConstraintAxisHorizontal;
    _downloadControlsRow.distribution = UIStackViewDistributionFill;
    _downloadControlsRow.spacing = kRowSpacing;
  }
  return _downloadControlsRow;
}

#pragma mark - UI Updates

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
                   multiplier:regularSizeClass
                                  ? kWidthConstraintRegularMultiplier
                                  : kWidthConstraintCompactMultiplier];

  self.viewWidthConstraint.active = YES;
}

// Updates views according to the current state, and data received through the
// consumer interface.
- (void)updateViews {
  [self updateViewsVisibility];
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      [self updateViewsForStateNotStarted];
      break;
    case kDownloadManagerStateInProgress:
      [self updateViewsForStateInProgress];
      break;
    case kDownloadManagerStateSucceeded:
      [self updateViewsForStateSucceeded];
      break;
    case kDownloadManagerStateFailed:
      [self updateViewsForStateFailed];
      break;
    case kDownloadManagerStateFailedNotResumable:
      [self updateViewsForStateFailedNotResumable];
      break;
  }
}

// Updates views `hidden` attribute according to the current state.
- (void)updateViewsVisibility {
  const bool taskNotStarted = _state == kDownloadManagerStateNotStarted;
  const bool taskInProgress = _state == kDownloadManagerStateInProgress;
  const bool taskSucceeded = _state == kDownloadManagerStateSucceeded;
  const bool taskFailed = _state == kDownloadManagerStateFailed;
  const bool destinationIsFiles =
      _downloadFileDestination == DownloadFileDestination::kFiles;
  const bool destinationIsDrive =
      _downloadFileDestination == DownloadFileDestination::kDrive;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.leadingIcon.hidden = taskNotStarted;
#else
  self.leadingIcon.hidden = YES;
#endif

  // Views only shown when task has not started.
  self.downloadToFilesButton.hidden = !taskNotStarted;
  self.downloadToDriveButton.hidden =
      !taskNotStarted || !_downloadToDriveButtonVisible;

  // Views only shown when task is in progress.
  self.progressView.hidden = !taskInProgress;
  self.filesProgressIcon.hidden = !taskInProgress || !destinationIsFiles;
  self.driveProgressIcon.hidden = !taskInProgress || !destinationIsDrive;

  // Views only shown when task has succeeded.
  self.openInButton.hidden = !taskSucceeded || !destinationIsFiles;
  self.openInDriveButton.hidden =
      !taskSucceeded || !destinationIsDrive || _installDriveButtonVisible;
  self.installAppButton.hidden =
      !taskSucceeded || !destinationIsDrive || !_installDriveButtonVisible;

  // Views only shown when task has failed.
  self.tryAgainButton.hidden = !taskFailed;
}

// Sets up views for the state `kDownloadManagerStateNotStarted`.
- (void)updateViewsForStateNotStarted {
  self.statusLabel.text =
      _countOfBytesExpectedToReceive == -1
          ? l10n_util::GetNSString(
                IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_FILE_WITHOUT_SIZE)
          : l10n_util::GetNSStringF(
                IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_FILE_WITH_SIZE,
                base::SysNSStringToUTF16(
                    GetSizeString(_countOfBytesExpectedToReceive)));

  if (base::FeatureList::IsEnabled(kIOSIncognitoDownloadsWarning) &&
      self.incognito) {
    self.detailLabel.text =
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_INCOGNITO_WARNING_MESSAGE);
    // Set to '0' to ensure the entire incognito warning is visible.
    self.detailLabel.numberOfLines = 0;
  } else {
    self.detailLabel.text = _fileName;
    self.detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
    self.detailLabel.numberOfLines = 1;
  }

  NSString* downloadToFilesButtonText =
      _downloadToDriveButtonVisible
          ? l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_TO_FILES)
          : [l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD)
                localizedUppercaseString];
  self.downloadToFilesButton.configuration = CreateDownloadButtonConfiguration(
      downloadToFilesButtonText, DownloadFileDestination::kFiles,
      _downloadToDriveButtonVisible,
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
  self.downloadToDriveButton.configuration = CreateDownloadButtonConfiguration(
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_TO_DRIVE),
      DownloadFileDestination::kDrive, true,
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark);
}

// Sets up views for the state `kDownloadManagerStateInProgress`.
- (void)updateViewsForStateInProgress {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination, true);

  switch (_downloadFileDestination) {
      // File is being downloaded to local Downloads folder.
    case DownloadFileDestination::kFiles: {
      std::u16string size =
          base::SysNSStringToUTF16(GetSizeString(_countOfBytesReceived));
      self.statusLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_DOWNLOADING_ELIPSIS, size);

      self.detailLabel.text = _fileName;
      self.detailLabel.numberOfLines = 1;
      break;
    }
    // File is being downloaded, then uploaded to Drive.
    case DownloadFileDestination::kDrive: {
      if (_countOfBytesExpectedToReceive == -1) {
        self.statusLabel.text = _fileName;
        self.statusLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
      } else {
        NSString* size = GetSizeString(_countOfBytesExpectedToReceive);
        self.statusLabel.text =
            l10n_util::GetNSStringF(IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
                                    base::SysNSStringToUTF16(_fileName),
                                    base::SysNSStringToUTF16(size));
        self.statusLabel.numberOfLines = 0;
      }
      self.detailLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_SAVING_TO_DRIVE,
          base::SysNSStringToUTF16(_saveToDriveUserEmail));
      self.detailLabel.numberOfLines = 0;
      break;
    }
  }

  self.progressView.progress = _progress;
}

// Sets up views for the state `kDownloadManagerStateSucceeded`.
- (void)updateViewsForStateSucceeded {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination, true);
  switch (_downloadFileDestination) {
    // File was downloaded to local Downloads folder.
    case DownloadFileDestination::kFiles:
      self.statusLabel.text =
          l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD_COMPLETE);
      self.detailLabel.text = _fileName;
      self.detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
      break;
    // File was downloaded, then uploaded to Drive.
    case DownloadFileDestination::kDrive:
      if (_countOfBytesExpectedToReceive == -1) {
        self.statusLabel.text = _fileName;
        self.statusLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
      } else {
        NSString* size = GetSizeString(_countOfBytesExpectedToReceive);
        self.statusLabel.text =
            l10n_util::GetNSStringF(IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
                                    base::SysNSStringToUTF16(_fileName),
                                    base::SysNSStringToUTF16(size));
        self.statusLabel.numberOfLines = 0;
      }
      self.detailLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_SAVED_TO_DRIVE,
          base::SysNSStringToUTF16(_saveToDriveUserEmail));
      self.detailLabel.numberOfLines = 0;
      break;
  }
}

// Sets up views for the state `kDownloadManagerStateFailed`.
- (void)updateViewsForStateFailed {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination, true);
  self.statusLabel.text =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_COULDNT_DOWNLOAD);
  self.detailLabel.text = _fileName;
  self.detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
}

// Sets up views for the state `kDownloadManagerStateFailedNotResumable`.
- (void)updateViewsForStateFailedNotResumable {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination, true);
  self.statusLabel.text =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_CANNOT_BE_RETRIED);
  self.detailLabel.text = _fileName;
  self.detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
}

@end
