// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "remoting/ios/app/remoting_view_controller.h"

#include <SystemConfiguration/SystemConfiguration.h>
#include <netinet/in.h>

#import "base/bind.h"
#import "ios/third_party/material_components_ios/src/components/AnimationTiming/src/MaterialAnimationTiming.h"
#import "ios/third_party/material_components_ios/src/components/AppBar/src/MDCAppBarViewController.h"
#import "ios/third_party/material_components_ios/src/components/Dialogs/src/MaterialDialogs.h"
#import "ios/third_party/material_components_ios/src/components/ShadowElevations/src/MaterialShadowElevations.h"
#import "ios/third_party/material_components_ios/src/components/ShadowLayer/src/MaterialShadowLayer.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/client_connection_view_controller.h"
#import "remoting/ios/app/host_collection_view_controller.h"
#import "remoting/ios/app/host_fetching_error_view_controller.h"
#import "remoting/ios/app/host_fetching_view_controller.h"
#import "remoting/ios/app/host_setup_view_controller.h"
#import "remoting/ios/app/host_view_controller.h"
#import "remoting/ios/app/refresh_control_provider.h"
#import "remoting/ios/app/remoting_menu_view_controller.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"
#import "remoting/ios/domain/client_session_details.h"
#import "remoting/ios/facade/remoting_service.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/string_resources.h"
#include "remoting/client/connect_to_host_info.h"
#include "remoting/ios/facade/host_list_service.h"
#include "ui/base/l10n/l10n_util.h"

static CGFloat kHostInset = 5.f;

namespace {

#pragma mark - Network Reachability

enum class ConnectionType {
  UNKNOWN,
  NONE,
  WWAN,
  WIFI,
};

ConnectionType GetConnectionType() {
  // 0.0.0.0 is a special token that causes reachability to monitor the general
  // routing status of the device, both IPv4 and IPv6.
  struct sockaddr_in addr = {0};
  addr.sin_len = sizeof(addr);
  addr.sin_family = AF_INET;
  base::ScopedCFTypeRef<SCNetworkReachabilityRef> reachability(
      SCNetworkReachabilityCreateWithAddress(
          kCFAllocatorDefault, reinterpret_cast<struct sockaddr*>(&addr)));
  SCNetworkReachabilityFlags flags;
  BOOL success = SCNetworkReachabilityGetFlags(reachability, &flags);
  if (!success) {
    return ConnectionType::UNKNOWN;
  }
  BOOL isReachable = flags & kSCNetworkReachabilityFlagsReachable;
  BOOL needsConnection = flags & kSCNetworkReachabilityFlagsConnectionRequired;
  BOOL isNetworkReachable = isReachable && !needsConnection;

  if (!isNetworkReachable) {
    return ConnectionType::NONE;
  } else if (flags & kSCNetworkReachabilityFlagsIsWWAN) {
    return ConnectionType::WWAN;
  }
  return ConnectionType::WIFI;
}

}  // namespace

#pragma mark - RemotingViewController

using remoting::HostListService;

@interface RemotingViewController ()<HostCollectionViewControllerDelegate,
                                     UIViewControllerAnimatedTransitioning,
                                     UIViewControllerTransitioningDelegate> {
  MDCDialogTransitionController* _dialogTransitionController;
  MDCAppBarViewController* _appBarViewController;
  HostCollectionViewController* _collectionViewController;
  HostFetchingViewController* _fetchingViewController;
  HostFetchingErrorViewController* _fetchingErrorViewController;
  HostSetupViewController* _setupViewController;
  HostListService* _hostListService;
  std::unique_ptr<HostListService::CallbackSubscription>
      _hostListStateSubscription;
  std::unique_ptr<HostListService::CallbackSubscription>
      _hostListFetchFailureSubscription;

  NSArray<id<RemotingRefreshControl>>* _refreshControls;
}
@end

@implementation RemotingViewController

- (instancetype)init {
  UICollectionViewFlowLayout* layout =
      [[MDCCollectionViewFlowLayout alloc] init];
  layout.minimumInteritemSpacing = 0;
  CGFloat sectionInset = kHostInset * 2.f;
  [layout setSectionInset:UIEdgeInsetsMake(sectionInset, sectionInset,
                                           sectionInset, sectionInset)];
  self = [super init];
  if (self) {
    _hostListService = HostListService::GetInstance();

    __weak RemotingViewController* weakSelf = self;
    RemotingRefreshAction refreshAction = ^{
      [weakSelf didSelectRefresh];
    };

    _collectionViewController = [[HostCollectionViewController alloc]
        initWithCollectionViewLayout:layout];
    _collectionViewController.delegate = self;
    _collectionViewController.scrollViewDelegate = self.headerViewController;

    _fetchingViewController = [[HostFetchingViewController alloc] init];

    _fetchingErrorViewController =
        [[HostFetchingErrorViewController alloc] init];
    _fetchingErrorViewController.onRetryCallback = refreshAction;

    _setupViewController = [[HostSetupViewController alloc] init];
    _setupViewController.scrollViewDelegate = self.headerViewController;

    _appBarViewController = [[MDCAppBarViewController alloc] init];
    [self addChildViewController:_appBarViewController];

    self.navigationItem.title =
        l10n_util::GetNSString(IDS_PRODUCT_NAME).lowercaseString;

    UIBarButtonItem* menuButton =
        [[UIBarButtonItem alloc] initWithImage:RemotingTheme.menuIcon
                                         style:UIBarButtonItemStyleDone
                                        target:self
                                        action:@selector(didSelectMenu)];
    remoting::SetAccessibilityInfoFromImage(menuButton);
    self.navigationItem.leftBarButtonItem = menuButton;

    _appBarViewController.headerView.backgroundColor =
        RemotingTheme.hostListBackgroundColor;
    _appBarViewController.navigationBar.backgroundColor =
        RemotingTheme.hostListBackgroundColor;
    MDCNavigationBarTextColorAccessibilityMutator* mutator =
        [[MDCNavigationBarTextColorAccessibilityMutator alloc] init];
    [mutator mutate:_appBarViewController.navigationBar];

    MDCFlexibleHeaderView* headerView = self.headerViewController.headerView;
    headerView.backgroundColor = [UIColor clearColor];

    // Use a custom shadow under the flexible header.
    MDCShadowLayer* shadowLayer = [MDCShadowLayer layer];
    [headerView setShadowLayer:shadowLayer
        intensityDidChangeBlock:^(CALayer* layer, CGFloat intensity) {
          CGFloat elevation = MDCShadowElevationAppBar * intensity;
          [(MDCShadowLayer*)layer setElevation:elevation];
        }];

    _refreshControls = @[
      [[RefreshControlProvider instance]
          createForScrollView:_collectionViewController.collectionView
                  actionBlock:refreshAction],
      [[RefreshControlProvider instance]
          createForScrollView:_setupViewController.tableView
                  actionBlock:refreshAction],
    ];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIImage* image = [UIImage imageNamed:@"Background"];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  [self.view addSubview:imageView];
  [self.view sendSubviewToBack:imageView];

  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [[imageView widthAnchor]
        constraintGreaterThanOrEqualToAnchor:[self.view widthAnchor]],
    [[imageView heightAnchor]
        constraintGreaterThanOrEqualToAnchor:[self.view heightAnchor]],
  ]];

  [self.view addSubview:_appBarViewController.view];
  [_appBarViewController didMoveToParentViewController:self];

  __weak __typeof(self) weakSelf = self;
  _hostListStateSubscription =
      _hostListService->RegisterHostListStateCallback(base::BindRepeating(^{
        [weakSelf hostListStateDidChange];
      }));
  _hostListFetchFailureSubscription =
      _hostListService->RegisterFetchFailureCallback(base::BindRepeating(^{
        [weakSelf hostListFetchDidFail];
      }));
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Just in case the view controller misses the host list state event before
  // the listener is registered.
  [self refreshContent];
  _hostListService->RequestFetch();

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(applicationDidBecomeActive:)
             name:UIApplicationDidBecomeActiveNotification
           object:nil];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];

  [NSNotificationCenter.defaultCenter
      removeObserver:self
                name:UIApplicationDidBecomeActiveNotification
              object:nil];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - HostListService Callbacks

- (void)hostListStateDidChange {
  [self refreshContent];
}

- (void)hostListFetchDidFail {
  [self handleHostListFetchFailure];
}

#pragma mark - HostCollectionViewControllerDelegate

- (void)didSelectCell:(HostCollectionViewCell*)cell
           completion:(void (^)())completionBlock {
  if (![cell.hostInfo isOnline]) {
    MDCSnackbarMessage* message = [[MDCSnackbarMessage alloc] init];
    message.text = l10n_util::GetNSString(IDS_HOST_OFFLINE_TOOLTIP);
    [MDCSnackbarManager showMessage:message];
    return;
  }

  if (GetConnectionType() == ConnectionType::NONE) {
    [MDCSnackbarManager
        showMessage:[MDCSnackbarMessage
                        messageWithText:l10n_util::GetNSString(
                                            IDS_ERROR_NETWORK_ERROR)]];
    return;
  }

  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:nil];
  ClientConnectionViewController* clientConnectionViewController =
      [[ClientConnectionViewController alloc] initWithHostInfo:cell.hostInfo];
  [self.navigationController pushViewController:clientConnectionViewController
                                       animated:YES];

  completionBlock();
}

- (NSInteger)getHostCount {
  return _hostListService->hosts().size();
}

- (HostInfo*)getHostAtIndexPath:(NSIndexPath*)path {
  return [[HostInfo alloc]
      initWithRemotingHostInfo:_hostListService->hosts()[path.row]];
}

#pragma mark - UIViewControllerTransitioningDelegate

- (nullable id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  return self;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.2;
}

#pragma mark - Private

- (void)didSelectRefresh {
  _hostListService->RequestFetch();
}

- (void)didSelectMenu {
  [AppDelegate.instance showMenuAnimated:YES];
}

- (void)refreshContent {
  if (_hostListService->state() == HostListService::State::FETCHING) {
    // We don't need to show the fetching view when either the host list or the
    // setup view is already shown. Refresh control will handle the
    // user-triggered refresh, and we don't need to show anything if
    // that's a background refresh (e.g. user just closed the session).
    if (self.contentViewController != _collectionViewController &&
        self.contentViewController != _setupViewController) {
      self.contentViewController = _fetchingViewController;
    }
    return;
  }

  if (_hostListService->state() == HostListService::State::NOT_FETCHED) {
    if (!_hostListService->last_fetch_failure()) {
      self.contentViewController = nil;
    } else {
      // hostListFetchDidFailNotification might miss the first failure happened
      // before the notification is registered. This logic covers that.
      [self handleHostListFetchFailure];
    }
    return;
  }

  DCHECK(_hostListService->state() == HostListService::State::FETCHED);

  [self stopAllRefreshControls];

  if (_hostListService->hosts().size() > 0) {
    [_collectionViewController.collectionView reloadData];
    self.headerViewController.headerView.trackingScrollView =
        _collectionViewController.collectionView;
    self.contentViewController = _collectionViewController;
  } else {
    self.headerViewController.headerView.trackingScrollView =
        _setupViewController.tableView;
    self.contentViewController = _setupViewController;
  }
  self.contentViewController.view.frame = self.view.bounds;
}

- (void)handleHostListFetchFailure {
  const auto* failure = _hostListService->last_fetch_failure();
  if (!failure) {
    return;
  }
  NSString* errorText = base::SysUTF8ToNSString(failure->localized_description);
  if ([self isAnyRefreshControlRefreshing]) {
    // User could just try pull-to-refresh again to refresh. We just need to
    // show the error as a toast.
    [MDCSnackbarManager
        showMessage:[MDCSnackbarMessage messageWithText:errorText]];
    [self stopAllRefreshControls];
    return;
  }

  // Pull-to-refresh is not available. We need to show a dedicated view to allow
  // user to retry.

  // Dismiss snackbars and hide the SSO menu so that the accessibility focus
  // can shift into the label.
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:nil];
  [AppDelegate.instance hideMenuAnimated:YES];

  _fetchingErrorViewController.label.text = errorText;
  remoting::SetAccessibilityFocusElement(_fetchingErrorViewController.label);
  self.contentViewController = _fetchingErrorViewController;
}

- (BOOL)isAnyRefreshControlRefreshing {
  for (id<RemotingRefreshControl> control in _refreshControls) {
    if (control.isRefreshing) {
      return YES;
    }
  }
  return NO;
}

- (void)stopAllRefreshControls {
  for (id<RemotingRefreshControl> control in _refreshControls) {
    [control endRefreshing];
  }
}

- (void)applicationDidBecomeActive:(UIApplication*)application {
  _hostListService->RequestFetch();
}

@end
