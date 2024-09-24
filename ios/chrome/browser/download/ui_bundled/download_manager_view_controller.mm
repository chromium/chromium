// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui_bundled/download_manager_view_controller.h"

#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_constants.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_view_controller_delegate.h"
#import "ios/chrome/browser/download/ui_bundled/features.h"
#import "ios/chrome/browser/download/ui_bundled/radial_progress_view.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
// Names of icons used in Download buttons or as leading icon.
NSString* const kFilesAppWithBackgroundImage =
    @"apple_files_app_with_background";
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
constexpr CGFloat kRowSpacing = 12;

// Other UI elements constants.
constexpr CGFloat kLeadingIconSize = 24;
constexpr CGFloat kTextStackSpacing = 2;
constexpr CGFloat kProgressViewLineWidth = 2.5;
constexpr CGFloat kCloseButtonIconSize = 30;

// Where to put the action button depending on the layout.
constexpr int kButtonIndexInTextStack = 2;
constexpr int kButtonIndexInDownloadRowStack = 3;

// Animation constants for progress <-> button transition.
const NSTimeInterval kAnimationDelay = 0.5;
const NSTimeInterval kAnimationDuration = 0.15;
const CGFloat kAnimationMinScale = 0.75;

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

// Returns the appropriate image for a destination icon.
UIImage* GetDownloadFileDestinationImage(DownloadFileDestination destination) {
  UIImage* destination_image = nil;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  static dispatch_once_t once_token;
  static UIImage* files_image;
  static UIImage* drive_image;
  dispatch_once(&once_token, ^{
    files_image = [UIImage imageNamed:kFilesAppWithBackgroundImage];
    drive_image = [UIImage imageNamed:kDriveAppWithBackgroundImage];
  });

  switch (destination) {
    case DownloadFileDestination::kFiles:
      destination_image = files_image;
      break;
    case DownloadFileDestination::kDrive:
      destination_image = drive_image;
      break;
  }
#endif

  return destination_image;
}

// The font used for action buttons.
UIFont* ActionButtonFont() {
  UIFont* font = [UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];
  return [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
}

// Creates the title for an action button ("DOWNLOAD", "SAVE...", "OPEN", "GET
// THE APP" or "TRY AGAIN").
NSAttributedString* CreateActionButtonTitle(NSString* title) {
  return [[NSAttributedString alloc]
      initWithString:title
          attributes:@{NSFontAttributeName : ActionButtonFont()}];
}

// Creates an action button ("DOWNLOAD", "SAVE...", "OPEN", "GET THE APP" or
// "TRY AGAIN").
UIButton* CreateActionButton(NSString* title,
                             NSString* accessibility_identifier,
                             UIAction* action) {
  UIButtonConfiguration* conf =
      [UIButtonConfiguration plainButtonConfiguration];
  conf.attributedTitle = CreateActionButtonTitle(title);
  NSDirectionalEdgeInsets insets = conf.contentInsets;
  insets.leading = insets.trailing = 0;
  conf.contentInsets = insets;
  UIButton* button = [UIButton buttonWithConfiguration:conf
                                         primaryAction:action];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisVertical];
  button.accessibilityIdentifier = accessibility_identifier;
  button.pointerInteractionEnabled = YES;
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

@interface DownloadManagerViewController () <FullscreenUIElement> {
  NSString* _fileName;
  int64_t _countOfBytesReceived;
  int64_t _countOfBytesExpectedToReceive;
  float _progress;
  DownloadManagerState _state;
  DownloadManagerState _transitioningFromState;
  BOOL _installDriveButtonVisible;
  BOOL _multipleDestinationsAvailable;
  DownloadFileDestination _downloadFileDestination;
  NSString* _saveToDriveUserEmail;
  BOOL _addedConstraints;  // YES if NSLayoutConstraits were added.

  // Animation ivars.
  // Animation is in progress. New animation will be queued.
  BOOL _animating;
  // An animation was queued and will be executed at the end of the current
  // animation.
  BOOL _needsTransitioningToButton;
  BOOL _needsTransitioningToProgress;
  BOOL _canOpenFile;
}

@property(nonatomic, strong) UIImageView* leadingIcon;
@property(nonatomic, strong) UIImageView* leadingIconNotStarted;
@property(nonatomic, strong) UILabel* statusLabel;
@property(nonatomic, strong) UILabel* detailLabel;
@property(nonatomic, strong) UIStackView* textStack;
@property(nonatomic, strong) RadialProgressView* progressView;
@property(nonatomic, strong) UIImageView* filesProgressIcon;
@property(nonatomic, strong) UIImageView* driveProgressIcon;
@property(nonatomic, strong) UIButton* downloadButton;
@property(nonatomic, strong) UIButton* openButton;
@property(nonatomic, strong) UIButton* openInButton;
@property(nonatomic, strong) UIButton* openInDriveButton;
@property(nonatomic, strong) UIButton* installAppButton;
@property(nonatomic, strong) UIButton* tryAgainButton;
@property(nonatomic, strong) UIButton* closeButton;
@property(nonatomic, weak) UIButton* currentButton;
@property(nonatomic, strong) UIStackView* downloadControlsRow;

// Represents constraint for self.view.widthAnchor, which is anchored to
// superview with different multipliers depending on size class. Stored in a
// property to allow deactivating the old constraint.
@property(strong, nonatomic) NSLayoutConstraint* viewWidthConstraint;

// UILayoutGuide for adding bottom margin to Download Manager view.
@property(strong, nonatomic) UILayoutGuide* bottomMarginGuide;

@end

@implementation DownloadManagerViewController {
  // A FullscreenController to hide the UI along the toolbar.
  raw_ptr<FullscreenController> _fullscreenController;

  // Bridge to observe `_fullscreenController`.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}

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
  [self.downloadControlsRow addArrangedSubview:self.leadingIconNotStarted];
  [self.textStack addArrangedSubview:self.statusLabel];
  [self.textStack addArrangedSubview:self.detailLabel];
  [self.downloadControlsRow addArrangedSubview:self.textStack];
  [self.progressView addSubview:self.filesProgressIcon];
  [self.progressView addSubview:self.driveProgressIcon];
  [self.downloadControlsRow addArrangedSubview:self.progressView];
  [self.downloadControlsRow addArrangedSubview:self.closeButton];
  [self.view addSubview:self.downloadControlsRow];
  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.self ]
                       withAction:@selector(updateActionButtonLayout)];
  }

  self.bottomMarginGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:self.bottomMarginGuide];
  self.view.accessibilityElements = @[
    self.statusLabel,
    self.detailLabel,
    self.downloadButton,
    self.openButton,
    self.openInButton,
    self.openInDriveButton,
    self.installAppButton,
    self.tryAgainButton,
    self.progressView,
    self.closeButton,
  ];
}

- (void)updateViewConstraints {
  if (_addedConstraints) {
    [super updateViewConstraints];
    return;
  }

  [self updateActionButtonLayout];
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
  UIView* leadingIconNotStarted = self.leadingIconNotStarted;
  [NSLayoutConstraint activateConstraints:@[
    [leadingIcon.widthAnchor constraintEqualToConstant:kLeadingIconSize],
    [leadingIcon.heightAnchor constraintEqualToConstant:kLeadingIconSize],
    [leadingIconNotStarted.widthAnchor
        constraintEqualToConstant:kLeadingIconSize],
    [leadingIconNotStarted.heightAnchor
        constraintEqualToConstant:kLeadingIconSize],
  ]];

  // Add a constraint on the detail label so it take as few lines as possible.
  NSLayoutConstraint* widthConstraint = [self.detailLabel.widthAnchor
      constraintEqualToAnchor:self.textStack.widthAnchor];
  widthConstraint.priority = UILayoutPriorityDefaultHigh;
  widthConstraint.active = YES;

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
  if (_incognito != incognito) {
    _incognito = incognito;
    [self updateViews];
  }
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
    if (state == kDownloadManagerStateSucceeded) {
      // Some Download task may not report progress correctly, but animation
      // does not look good if progress is not at 1.
      [self setProgress:1];
    }
    _state = state;
    [self updateViews];
    _transitioningFromState = state;
  }
}

- (void)setInstallDriveButtonVisible:(BOOL)visible animated:(BOOL)animated {
  if (_installDriveButtonVisible != visible) {
    _installDriveButtonVisible = visible;
    [self updateViews];
  }
}

- (void)setMultipleDestinationsAvailable:(BOOL)multipleDestinationsAvailable {
  if (_multipleDestinationsAvailable != multipleDestinationsAvailable) {
    _multipleDestinationsAvailable = multipleDestinationsAvailable;
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

- (void)setCanOpenFile:(BOOL)canOpenFile {
  if (_canOpenFile == canOpenFile) {
    return;
  }
  _canOpenFile = canOpenFile;
  [self updateViews];
}

#pragma mark - DownloadManagerViewControllerProtocol

- (UIView*)openInSourceView {
  return self.openInButton;
}

- (void)setFullscreenController:(FullscreenController*)fullscreenController {
  if (_fullscreenController) {
    _fullscreenUIUpdater.reset();
    self.view.alpha = 1;
  }
  _fullscreenController = fullscreenController;
  if (_fullscreenController) {
    _fullscreenUIUpdater =
        std::make_unique<FullscreenUIUpdater>(_fullscreenController, self);
    [self updateForFullscreenProgress:_fullscreenController->GetProgress()];
  }
}

#pragma mark - UI elements

- (UIImageView*)leadingIcon {
  if (!_leadingIcon) {
    _leadingIcon = [[UIImageView alloc] init];
    _leadingIcon.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingIcon.contentMode = UIViewContentModeCenter;
    [_leadingIcon setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
  }

  return _leadingIcon;
}

- (UIImageView*)leadingIconNotStarted {
  if (!_leadingIconNotStarted) {
    _leadingIconNotStarted = [[UIImageView alloc] init];
    _leadingIconNotStarted.translatesAutoresizingMaskIntoConstraints = NO;
    _leadingIconNotStarted.contentMode = UIViewContentModeCenter;
    [_leadingIconNotStarted
        setContentHuggingPriority:UILayoutPriorityRequired
                          forAxis:UILayoutConstraintAxisHorizontal];
    _leadingIconNotStarted.image = DefaultSymbolTemplateWithPointSize(
        kOpenInDownloadsSymbol, kSymbolDownloadInfobarPointSize);
  }

  return _leadingIconNotStarted;
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
    [_detailLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                    forAxis:UILayoutConstraintAxisHorizontal];
    _detailLabel.translatesAutoresizingMaskIntoConstraints = NO;
  }

  return _detailLabel;
}

- (UIStackView*)textStack {
  if (!_textStack) {
    _textStack = [[UIStackView alloc] init];
    _textStack.translatesAutoresizingMaskIntoConstraints = NO;
    _textStack.axis = UILayoutConstraintAxisVertical;
    _textStack.spacing = kTextStackSpacing;
    _textStack.alignment = UIStackViewAlignmentLeading;
    [_textStack
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_textStack setContentHuggingPriority:UILayoutPriorityRequired
                                  forAxis:UILayoutConstraintAxisVertical];
  }

  return _textStack;
}

- (UIButton*)downloadButton {
  if (!_downloadButton) {
    __weak __typeof(self) weakSelf = self;
    _downloadButton = CreateActionButton(
        [l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD)
            localizedUppercaseString],
        kDownloadManagerDownloadAccessibilityIdentifier,
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              downloadManagerViewControllerDidStartDownload:weakSelf];
        }]);
  }

  return _downloadButton;
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

- (UIButton*)openButton {
  if (!_openButton) {
    __weak __typeof(self) weakSelf = self;
    _openButton = CreateActionButton(
        [l10n_util::GetNSString(IDS_IOS_OPEN_PDF) localizedUppercaseString],
        kDownloadManagerOpenAccessibilityIdentifier,
        [UIAction actionWithHandler:^(UIAction* action) {
          [weakSelf.delegate
              openDownloadedFileForDownloadManagerViewController:weakSelf];
        }]);
  }

  return _openButton;
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
    _openInDriveButton.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_DOWNLOAD_MANAGER_OPEN_ACCESSIBILITY_LABEL);
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
    _closeButton.accessibilityIdentifier =
        kDownloadManagerCloseButtonAccessibilityIdentifier;
    [_closeButton.widthAnchor constraintEqualToConstant:kCloseButtonIconSize]
        .active = YES;
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

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [self updateActionButtonLayout];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  if (@available(iOS 17, *)) {
    return;
  }
  [self updateActionButtonLayout];
}
#endif

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
  self.overrideUserInterfaceStyle = self.incognito
                                        ? UIUserInterfaceStyleDark
                                        : UIUserInterfaceStyleUnspecified;
}

// Return the button that should be visible according to the current state, if
// any.
- (UIButton*)currentVisibleButton {
  switch (_state) {
    case kDownloadManagerStateNotStarted:
      return self.downloadButton;
    case kDownloadManagerStateSucceeded:
      switch (_downloadFileDestination) {
        case DownloadFileDestination::kFiles:
          return (base::FeatureList::IsEnabled(kDownloadedPDFOpening) &&
                  _canOpenFile)
                     ? self.openButton
                     : self.openInButton;
        case DownloadFileDestination::kDrive:
          return _installDriveButtonVisible ? self.installAppButton
                                            : self.openInDriveButton;
      }
    case kDownloadManagerStateFailed:
      return self.tryAgainButton;
    case kDownloadManagerStateInProgress:
    case kDownloadManagerStateFailedNotResumable:
      return nil;
  }
}

// Updates what button is visible according to `_state`.
- (void)updateCurrentVisibleButton {
  UIButton* currentButton = [self currentVisibleButton];
  if (currentButton != _currentButton) {
    [_currentButton removeFromSuperview];
    _currentButton = currentButton;
    [self updateActionButtonLayout];
    // Reset possibly animated properties in case an animation was interrupted.
    _currentButton.hidden = NO;
    [self animateSetView:_currentButton hidden:NO];
  }
}

// Updates views `hidden` attribute according to the current state.
- (void)updateViewsVisibility {
  const bool taskNotStarted = _state == kDownloadManagerStateNotStarted;
  const bool taskWasInProgress =
      _transitioningFromState == kDownloadManagerStateInProgress;
  const bool taskInProgress = _state == kDownloadManagerStateInProgress;
  const bool destinationIsFiles =
      _downloadFileDestination == DownloadFileDestination::kFiles;
  const bool destinationIsDrive =
      _downloadFileDestination == DownloadFileDestination::kDrive;

  self.leadingIconNotStarted.hidden = !taskNotStarted;

#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.leadingIcon.hidden = taskNotStarted;
#else
  self.leadingIcon.hidden = YES;
#endif

  if (taskWasInProgress && !taskInProgress) {
    // ProgressView -> Button.
    [self animateProgressViewToButton];
  } else if (!taskWasInProgress && taskInProgress) {
    // Button -> ProgressView.
    [self animateButtonToProgressView];
  } else if (!_animating) {
    // Anything else, just update without animation.
    [self updateCurrentVisibleButton];
    self.progressView.hidden = !taskInProgress;
    self.filesProgressIcon.hidden = !taskInProgress || !destinationIsFiles;
    self.driveProgressIcon.hidden = !taskInProgress || !destinationIsDrive;
  }
}

// Sets up views for the state `kDownloadManagerStateNotStarted`.
- (void)updateViewsForStateNotStarted {
  // Update status label text.
  self.statusLabel.text = [self localizedFileNameAndSizeWithPeriod:NO];
  // Update detail label text.
  if (self.incognito) {
    self.detailLabel.text =
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_INCOGNITO_WARNING_MESSAGE);
    // Set to '0' to ensure the entire incognito warning is visible.
    self.detailLabel.numberOfLines = 0;
  } else {
    // The detail label has no text to display.
    self.detailLabel.text = nil;
  }

  // Update title and accessibility identifier of download button.
  UIButtonConfiguration* downloadButtonConfiguration =
      self.downloadButton.configuration;
  if (_multipleDestinationsAvailable) {
    downloadButtonConfiguration.attributedTitle = CreateActionButtonTitle(
        l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_SAVE_ELLIPSIS));
    self.downloadButton.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_DOWNLOAD_MANAGER_SAVE_ACCESSIBILITY_LABEL);

    self.downloadButton.accessibilityIdentifier =
        kDownloadManagerSaveEllipsisAccessibilityIdentifier;
  } else {
    downloadButtonConfiguration.attributedTitle = CreateActionButtonTitle(
        [l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_DOWNLOAD)
            localizedUppercaseString]);
    self.downloadButton.accessibilityLabel = nil;
    self.downloadButton.accessibilityIdentifier =
        kDownloadManagerDownloadAccessibilityIdentifier;
  }
  self.downloadButton.configuration = downloadButtonConfiguration;
  self.closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_MANAGER_CLOSE_DOWNLOAD_ACCESSIBILITY_LABEL);
}

// Sets up views for the state `kDownloadManagerStateInProgress`.
- (void)updateViewsForStateInProgress {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination);

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
      self.statusLabel.text = [self localizedFileNameAndSizeWithPeriod:YES];
      self.detailLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_SAVING_TO_DRIVE,
          base::SysNSStringToUTF16(_saveToDriveUserEmail));
      self.detailLabel.numberOfLines = 0;
      break;
    }
  }
  self.closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_MANAGER_CANCEL_DOWNLOAD_ACCESSIBILITY_LABEL);

  self.progressView.progress = _progress;
}

// Sets up views for the state `kDownloadManagerStateSucceeded`.
- (void)updateViewsForStateSucceeded {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination);
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
      self.statusLabel.text = [self localizedFileNameAndSizeWithPeriod:YES];
      self.detailLabel.text = l10n_util::GetNSStringF(
          IDS_IOS_DOWNLOAD_MANAGER_SAVED_TO_DRIVE,
          base::SysNSStringToUTF16(_saveToDriveUserEmail));
      self.detailLabel.numberOfLines = 0;
      break;
  }
  self.closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_MANAGER_CLOSE_DOWNLOAD_ACCESSIBILITY_LABEL);
}

// Sets up views for the state `kDownloadManagerStateFailed`.
- (void)updateViewsForStateFailed {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination);
  self.statusLabel.text =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_COULDNT_DOWNLOAD);
  self.detailLabel.text = _fileName;
  self.detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  self.closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_MANAGER_CLOSE_DOWNLOAD_ACCESSIBILITY_LABEL);
}

// Sets up views for the state `kDownloadManagerStateFailedNotResumable`.
- (void)updateViewsForStateFailedNotResumable {
  self.leadingIcon.image =
      GetDownloadFileDestinationImage(_downloadFileDestination);
  self.statusLabel.text =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_CANNOT_BE_RETRIED);
  self.detailLabel.text = _fileName;
  self.detailLabel.lineBreakMode = NSLineBreakByTruncatingMiddle;
  self.closeButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_MANAGER_CLOSE_DOWNLOAD_ACCESSIBILITY_LABEL);
}

// Check where to put the action button.
// Rule are that button must be on one line, but should not take more than
// a third of the text area.
// Otherwise, it is put under on its own row.
- (void)updateActionButtonLayout {
  if (_currentButton) {
    // Compute the width of the button string.
    // It is not possible to use `UIButton intrinsicContentSize` or
    // `NSAttributedString size` are both those may return multi line size.
    // Instead get the raw string and recompoute its size.
    // Add the button insets to compute the button size.
    CGSize stringSize = [[_currentButton.configuration.attributedTitle string]
        sizeWithAttributes:@{NSFontAttributeName : ActionButtonFont()}];
    CGFloat stringWidth = ceil(stringSize.width);
    stringWidth += _currentButton.configuration.contentInsets.leading +
                   _currentButton.configuration.contentInsets.trailing;

    // This is the available text size.
    CGFloat availableWidth =
        self.view.frame.size.width -
        (kLeadingIconSize + kCloseButtonIconSize + 2 * kRowSpacing);

    BOOL isSmallButton = stringWidth < availableWidth / 3;
    UIView* buttonSuperview =
        isSmallButton ? self.downloadControlsRow : self.textStack;

    if (_currentButton.superview != buttonSuperview) {
      // Button needs to be moved.
      // Remove from the superview as insertArrangedSubview will not do it.
      [_currentButton removeFromSuperview];
      if (isSmallButton) {
        [self.downloadControlsRow
            insertArrangedSubview:_currentButton
                          atIndex:kButtonIndexInDownloadRowStack];
      } else {
        [self.textStack insertArrangedSubview:_currentButton
                                      atIndex:kButtonIndexInTextStack];
      }
    }
  }
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  CGFloat alphaValue = fmax((progress - 0.85) / 0.15, 0);
  self.view.alpha = alphaValue;
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled) {
    [self updateForFullscreenProgress:1];
  }
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  __weak __typeof(self) weakSelf = self;
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [weakSelf updateForFullscreenProgress:finalProgress];
  }];
}

#pragma mark - Animations

// Sets the property of `view` to make it `hidden` or not with animation.
- (void)animateSetView:(UIView*)view hidden:(BOOL)hidden {
  if (hidden) {
    view.transform =
        CGAffineTransformMakeScale(kAnimationMinScale, kAnimationMinScale);
    view.alpha = 0;
  } else {
    view.transform = CGAffineTransformMakeScale(1, 1);
    view.alpha = 1;
  }
}

// Sets the properties of the progress views to make them `hidden` or not
// with animation.
- (void)animateSetProgressViewHidden:(BOOL)hidden {
  [self animateSetView:self.progressView hidden:hidden];
  [self animateSetView:self.driveProgressIcon hidden:hidden];
  [self animateSetView:self.filesProgressIcon hidden:hidden];
}

// Helper for progress view -> button animation.
// Mid point function to toggle the visibility of views.
- (void)animateProgressViewToButtonToggleVisibility {
  // Restore animated properties.
  [self animateSetProgressViewHidden:NO];
  self.filesProgressIcon.tintColor = [UIColor colorNamed:kTextQuaternaryColor];
  self.driveProgressIcon.tintColor = [UIColor colorNamed:kTextQuaternaryColor];

  [self updateCurrentVisibleButton];
  self.currentButton.hidden = NO;
  [self animateSetView:self.currentButton hidden:YES];
  self.progressView.hidden = YES;
  self.filesProgressIcon.hidden = YES;
  self.driveProgressIcon.hidden = YES;
}

// Helper for progress view -> button animation.
// Called with progress view is hidden.
- (void)animateProgressViewToButtonHideProgressViewDidHide {
  [self animateProgressViewToButtonToggleVisibility];
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
      delay:0.0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        [weakSelf animateSetView:weakSelf.currentButton hidden:NO];
      }
      completion:^(BOOL secondFinished) {
        [weakSelf animationDone];
      }];
}

// Triggers an animation to hide the progress view and show the current button.
- (void)animateProgressViewToButton {
  if (_animating) {
    _needsTransitioningToButton = YES;
    _needsTransitioningToProgress = NO;
    return;
  }
  _animating = YES;
  // Turn the button blue to mark completion.
  self.filesProgressIcon.tintColor = [UIColor colorNamed:kBlueColor];
  self.driveProgressIcon.tintColor = [UIColor colorNamed:kBlueColor];
  [self currentVisibleButton].hidden = YES;

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock hideProgressView = ^{
    [weakSelf animateSetProgressViewHidden:YES];
  };

  [UIView
      animateWithDuration:kAnimationDuration
                    delay:kAnimationDelay
                  options:UIViewAnimationOptionCurveEaseIn
               animations:hideProgressView
               completion:^(BOOL finished) {
                 [weakSelf animateProgressViewToButtonHideProgressViewDidHide];
               }];
}

// Helper for button -> progress view animation.
// Mid point function to toggle the visibility of views.
- (void)animateButtonToProgressViewToggleVisibility {
  // Restore animated properties.
  [self animateSetView:self.currentButton hidden:NO];
  self.currentButton.hidden = NO;
  [self updateCurrentVisibleButton];
  if (_needsTransitioningToButton) {
    // If there is a new button, it will only appear after the next animation.
    // mark it hidden for now.
    self.currentButton.hidden = YES;
  }
  self.progressView.hidden = NO;
  const bool destinationIsFiles =
      _downloadFileDestination == DownloadFileDestination::kFiles;
  const bool destinationIsDrive =
      _downloadFileDestination == DownloadFileDestination::kDrive;
  self.filesProgressIcon.hidden = !destinationIsFiles;
  self.driveProgressIcon.hidden = !destinationIsDrive;
  [self animateSetProgressViewHidden:YES];
}

// Helper for button -> progress view animation.
// Called when button was hidden.
- (void)animateButtonToProgressViewButtonDidHide {
  [self animateProgressViewToButtonToggleVisibility];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock showProgress = ^{
    [weakSelf animateSetProgressViewHidden:NO];
  };
  [UIView animateWithDuration:kAnimationDuration
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:showProgress
                   completion:^(BOOL secondFinished) {
                     [weakSelf animationDone];
                   }];
}

// Triggers an animation to hide the current button and show the progress view.
- (void)animateButtonToProgressView {
  if (_animating) {
    _needsTransitioningToProgress = YES;
    _needsTransitioningToButton = NO;
    return;
  }
  _animating = YES;
  __weak __typeof(self) weakSelf = self;

  ProceduralBlock hideButton = ^{
    [weakSelf animateSetView:weakSelf.currentButton hidden:YES];
  };

  [UIView animateWithDuration:kAnimationDuration
                        delay:0.0
                      options:UIViewAnimationOptionCurveEaseIn
                   animations:hideButton
                   completion:^(BOOL finished) {
                     [weakSelf animateButtonToProgressViewButtonDidHide];
                   }];
}

// Called when an animation between progress view and current button ends.
- (void)animationDone {
  _animating = NO;
  if (_needsTransitioningToProgress) {
    _needsTransitioningToProgress = NO;
    [self animateButtonToProgressView];
  } else if (_needsTransitioningToButton) {
    _needsTransitioningToButton = NO;
    [self animateProgressViewToButton];
  } else {
    [self updateViews];
  }
}

#pragma mark - Private

// Returns a localized string with the file name as well as its size if it is
// available. If `period` is YES, then a period is appended at the end.
- (NSString*)localizedFileNameAndSizeWithPeriod:(BOOL)period {
  if (_countOfBytesExpectedToReceive == -1) {
    return period ? l10n_util::GetNSStringF(
                        IDS_IOS_DOWNLOAD_MANAGER_FILENAME_PERIOD,
                        base::SysNSStringToUTF16(_fileName))
                  : _fileName;
  }
  NSString* fileSize = GetSizeString(_countOfBytesExpectedToReceive);
  return l10n_util::GetNSStringF(
      period ? IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE_PERIOD
             : IDS_IOS_DOWNLOAD_MANAGER_FILENAME_WITH_SIZE,
      base::SysNSStringToUTF16(_fileName), base::SysNSStringToUTF16(fileSize));
}

@end
