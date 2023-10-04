// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/model/upgrade_center.h"

#import <memory>
#import <set>
#import <utility>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

@interface UpgradeCenter ()
// Creates infobars on all tabs.
- (void)showUpgradeInfoBars;
// Removes all the infobars.
- (void)hideUpgradeInfoBars;
// Callback when an infobar is closed, for any reason. Perform upgrade is set to
// YES if the user choose to upgrade.
- (void)dismissedInfoBar:(NSString*)tabId performUpgrade:(BOOL)shouldUpgrade;
// Returns YES if the infobar should be shown.
- (BOOL)shouldShowInfoBar;
// Returns YES if the last version signaled by a server side service is more
// recent than the current version.
- (BOOL)isCurrentVersionObsolete;
// Returns YES if the infobar has already been shown within the allowed display
// interval.
- (BOOL)infoBarShownRecently;
// Called when the application become active again.
- (void)applicationWillEnterForeground:(NSNotification*)note;
// The command handler for this object.
@property(nonatomic, weak) id<ApplicationCommands> handler;
@end

namespace {

// The amount of time that must elapse before showing the infobar again.
constexpr base::TimeDelta kInfobarDisplayInterval = base::Days(1);

// The class controlling the look of the infobar displayed when an upgrade is
// available.
class UpgradeInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  UpgradeInfoBarDelegate() : trigger_upgrade_(false) {}

  UpgradeInfoBarDelegate(const UpgradeInfoBarDelegate&) = delete;
  UpgradeInfoBarDelegate& operator=(const UpgradeInfoBarDelegate&) = delete;

  ~UpgradeInfoBarDelegate() override {}

  // Returns true is the infobar was closed by pressing the accept button.
  bool AcceptPressed() { return trigger_upgrade_; }

  void RemoveSelf() {
    infobars::InfoBar* infobar = this->infobar();
    if (infobar) {
      infobar->RemoveSelf();
    }
  }

 private:
  InfoBarIdentifier GetIdentifier() const override {
    return UPGRADE_INFOBAR_DELEGATE_IOS;
  }

  bool ShouldExpire(const NavigationDetails& details) const override {
    return false;
  }

  ui::ImageModel GetIcon() const override {
    if (icon_.IsEmpty()) {
      icon_ = gfx::Image(DefaultSymbolWithPointSize(kInfoCircleSymbol,
                                                    kInfobarSymbolPointSize));
    }
    return ui::ImageModel::FromImage(icon_);
  }

  std::u16string GetMessageText() const override {
    return l10n_util::GetStringUTF16(IDS_IOS_UPGRADE_AVAILABLE);
  }

  bool Accept() override {
    trigger_upgrade_ = true;
    return true;
  }

  int GetButtons() const override { return BUTTON_OK; }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    DCHECK(button == BUTTON_OK);
    return l10n_util::GetStringUTF16(IDS_IOS_UPGRADE_AVAILABLE_BUTTON);
  }

  mutable gfx::Image icon_;
  bool trigger_upgrade_;
};

// The InfoBarDelegate unfortunately is not called at all when an infoBar is
// simply dismissed. In order to catch that case this object listens to the
// infobars::InfoBarManager::Observer::OnInfoBarRemoved() which is invoked when
// an infobar is closed, for any reason.
class UpgradeInfoBarDismissObserver
    : public infobars::InfoBarManager::Observer {
 public:
  UpgradeInfoBarDismissObserver()
      : infobar_delegate_(nullptr), dismiss_delegate_(nil) {}

  UpgradeInfoBarDismissObserver(const UpgradeInfoBarDismissObserver&) = delete;
  UpgradeInfoBarDismissObserver& operator=(
      const UpgradeInfoBarDismissObserver&) = delete;

  ~UpgradeInfoBarDismissObserver() override {}

  void RegisterObserver(infobars::InfoBarManager* infobar_manager,
                        UpgradeInfoBarDelegate* infobar_delegate,
                        NSString* tab_id,
                        UpgradeCenter* dismiss_delegate) {
    scoped_observation_.Observe(infobar_manager);
    infobar_delegate_ = infobar_delegate;
    dismiss_delegate_ = dismiss_delegate;
    tab_id_ = [tab_id copy];
  }

  UpgradeInfoBarDelegate* infobar_delegate() { return infobar_delegate_; }

 private:
  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    if (infobar->delegate() == infobar_delegate_) {
      [dismiss_delegate_ dismissedInfoBar:tab_id_
                           performUpgrade:infobar_delegate_->AcceptPressed()];
    }
  }

  void OnManagerShuttingDown(
      infobars::InfoBarManager* infobar_manager) override {
    DCHECK(scoped_observation_.IsObservingSource(infobar_manager));
    scoped_observation_.Reset();
  }

  UpgradeInfoBarDelegate* infobar_delegate_;
  __weak UpgradeCenter* dismiss_delegate_;
  __strong NSString* tab_id_;
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      scoped_observation_{this};
};

}  // namespace

// The delegateHolder is a simple wrapper to be able to store all the
// infoBarDelegate related information in an object that can be put in
// an ObjectiveC container.
@interface DelegateHolder : NSObject {
  UpgradeInfoBarDismissObserver _observer;
}

- (instancetype)initWithInfoBarManager:(infobars::InfoBarManager*)infoBarManager
                       infoBarDelegate:(UpgradeInfoBarDelegate*)infoBarDelegate
                         upgradeCenter:(UpgradeCenter*)upgradeCenter
                                 tabId:(NSString*)tabId;

@property(nonatomic, readonly) UpgradeInfoBarDelegate* infoBarDelegate;
@end

@implementation DelegateHolder

- (instancetype)initWithInfoBarManager:(infobars::InfoBarManager*)infoBarManager
                       infoBarDelegate:(UpgradeInfoBarDelegate*)infoBarDelegate
                         upgradeCenter:(UpgradeCenter*)upgradeCenter
                                 tabId:(NSString*)tabId {
  self = [super init];
  if (self) {
    _observer.RegisterObserver(infoBarManager, infoBarDelegate, tabId,
                               upgradeCenter);
  }
  return self;
}

- (UpgradeInfoBarDelegate*)infoBarDelegate {
  return _observer.infobar_delegate();
}

@end

@implementation UpgradeCenter {
  // YES if the infobars are currently visible.
  BOOL _upgradeInfoBarIsVisible;
  // Used to store the visible upgrade infobars, indexed by tabId.
  __strong NSMutableDictionary<NSString*, DelegateHolder*>*
      _upgradeInfoBarDelegates;
  // Stores the clients of the upgrade center. These objectiveC objects are not
  // retained.
  __strong NSHashTable<id<UpgradeCenterClient>>* _clients;
#if DCHECK_IS_ON()
  BOOL _inCallback;
#endif
}
@synthesize handler = _handler;

+ (UpgradeCenter*)sharedInstance {
  static UpgradeCenter* obj;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    obj = [[self alloc] init];
  });
  return obj;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _upgradeInfoBarDelegates = [[NSMutableDictionary alloc] init];

    // There is no dealloc and no unregister as this class is a never
    // deallocated singleton.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillEnterForeground:)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];

    _upgradeInfoBarIsVisible = [self shouldShowInfoBar];
    _clients = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (BOOL)shouldShowInfoBar {
  return [self isCurrentVersionObsolete] && ![self infoBarShownRecently];
}

- (BOOL)isCurrentVersionObsolete {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* nextVersion = [defaults stringForKey:kIOSChromeNextVersionKey];
  if (nextVersion) {
    const base::Version& current_version = version_info::GetVersion();
    const std::string upgrade = base::SysNSStringToUTF8(nextVersion);
    return current_version < base::Version(upgrade);
  }
  return NO;
}

- (BOOL)infoBarShownRecently {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDate* lastDisplayDate = base::apple::ObjCCast<NSDate>(
      [defaults objectForKey:kLastInfobarDisplayTimeKey]);
  if (!lastDisplayDate) {
    return NO;
  }

  // Absolute value is to ensure the infobar won't be suppressed forever if the
  // clock temporarily jumps to the distant future.
  const base::Time lastDisplayTime = base::Time::FromNSDate(lastDisplayDate);
  return (base::Time::Now() - lastDisplayTime).magnitude() <
         kInfobarDisplayInterval;
}

- (void)applicationWillEnterForeground:(NSNotification*)note {
  if (_upgradeInfoBarIsVisible) {
    return;
  }

  // When returning to active if the upgrade notification has been dismissed,
  // bring it back.
  if ([self shouldShowInfoBar]) {
    [self showUpgradeInfoBars];
  }
}

- (void)registerClient:(id<UpgradeCenterClient>)client
           withHandler:(id<ApplicationCommands>)handler {
  [_clients addObject:client];
  self.handler = handler;
  if (_upgradeInfoBarIsVisible) {
    [client showUpgrade:self];
  }
}

- (void)unregisterClient:(id<UpgradeCenterClient>)client {
#if DCHECK_IS_ON()
  DCHECK(!_inCallback);
#endif
  [_clients removeObject:client];
}

- (void)addInfoBarToManager:(infobars::InfoBarManager*)infoBarManager
                   forTabId:(NSString*)tabId {
  DCHECK(tabId);
  DCHECK(infoBarManager);

  // Nothing to do if the infobar are not visible at this point in time.
  if (!_upgradeInfoBarIsVisible) {
    return;
  }

  // Nothing to do if the infobar is already there.
  if ([_upgradeInfoBarDelegates objectForKey:tabId]) {
    return;
  }

  auto infobarDelegate = std::make_unique<UpgradeInfoBarDelegate>();
  DelegateHolder* delegateHolder =
      [[DelegateHolder alloc] initWithInfoBarManager:infoBarManager
                                     infoBarDelegate:infobarDelegate.get()
                                       upgradeCenter:self
                                               tabId:tabId];

  [_upgradeInfoBarDelegates setObject:delegateHolder forKey:tabId];
  infoBarManager->AddInfoBar(CreateConfirmInfoBar(std::move(infobarDelegate)));
}

- (void)tabWillClose:(NSString*)tabId {
  [_upgradeInfoBarDelegates removeObjectForKey:tabId];
}

- (void)dismissedInfoBar:(NSString*)tabId performUpgrade:(BOOL)shouldUpgrade {
  // If the tabId is not in the upgradeInfoBarDelegates_ just ignore the
  // notification. In all likelihood it was trigerred by calling
  // -hideUpgradeInfoBars. Or because a tab was closed without dismissing the
  // infobar.
  DelegateHolder* delegateHolder =
      [_upgradeInfoBarDelegates objectForKey:tabId];
  if (!delegateHolder) {
    return;
  }

  // Forget about this dismissed infobar.
  [_upgradeInfoBarDelegates removeObjectForKey:tabId];

  // Get rid of all the infobars on the other tabs.
  [self hideUpgradeInfoBars];

  if (shouldUpgrade) {
    NSString* urlString = [[NSUserDefaults standardUserDefaults]
        valueForKey:kIOSChromeUpgradeURLKey];
    if (!urlString) {
      return;  // Missing URL, no upgrade possible.
    }

    GURL URL = GURL(base::SysNSStringToUTF8(urlString));
    if (!URL.is_valid()) {
      return;
    }

    if (web::UrlHasWebScheme(URL)) {
      // This URL can be opened in the application, just open in a new tab.
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithURLFromChrome:URL];
      [self.handler openURLInNewTab:command];
    } else {
      // This URL scheme is not understood, ask the system to open it.
      NSURL* launchURL = [NSURL URLWithString:urlString];
      if (launchURL) {
        [[UIApplication sharedApplication] openURL:launchURL
                                           options:@{}
                                 completionHandler:nil];
      }
    }
  }
}

- (void)showUpgradeInfoBars {
// Add an infobar on all the open tabs.
#if DCHECK_IS_ON()
  _inCallback = YES;
#endif
  _upgradeInfoBarIsVisible = YES;
  for (id<UpgradeCenterClient> upgradeClient in _clients) {
    [upgradeClient showUpgrade:self];
  }
#if DCHECK_IS_ON()
  _inCallback = NO;
#endif

  [[NSUserDefaults standardUserDefaults] setObject:[NSDate date]
                                            forKey:kLastInfobarDisplayTimeKey];
}

- (void)hideUpgradeInfoBars {
  _upgradeInfoBarIsVisible = NO;
  // It is important to call -allKeys here and not using a fast iteration on the
  // dictionary directly: the dictionary is modified as we go...
  for (NSString* tabId in [_upgradeInfoBarDelegates allKeys]) {
    // It is important to retain the delegateHolder as otherwise it is
    // deallocated as soon as it is removed from the dictionary.
    DelegateHolder* delegateHolder =
        [_upgradeInfoBarDelegates objectForKey:tabId];
    if (delegateHolder) {
      [_upgradeInfoBarDelegates removeObjectForKey:tabId];
      UpgradeInfoBarDelegate* delegate = [delegateHolder infoBarDelegate];
      DCHECK(delegate);
      delegate->RemoveSelf();
    }
  }
}

- (void)upgradeNotificationDidOccur:(const UpgradeRecommendedDetails&)details {
  const GURL& upgradeUrl = details.upgrade_url;

  if (!upgradeUrl.is_valid()) {
    // The application may crash if the URL is invalid. As the URL is defined
    // externally to the application it needs to bail right away and ignore the
    // upgrade notification.
    NOTREACHED();
    return;
  }

  if (!details.next_version.size() ||
      !base::Version(details.next_version).IsValid()) {
    // If the upgrade version is not known or is invalid just ignore the
    // upgrade notification.
    NOTREACHED();
    return;
  }

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Reset the display clock when the version changes.
  NSString* newVersionString = base::SysUTF8ToNSString(details.next_version);
  NSString* previousVersionString =
      [defaults stringForKey:kIOSChromeNextVersionKey];
  if (!previousVersionString ||
      ![previousVersionString isEqualToString:newVersionString]) {
    [defaults removeObjectForKey:kLastInfobarDisplayTimeKey];
  }

  [defaults setValue:base::SysUTF8ToNSString(upgradeUrl.spec())
              forKey:kIOSChromeUpgradeURLKey];
  [defaults setValue:newVersionString forKey:kIOSChromeNextVersionKey];

  if ([self shouldShowInfoBar]) {
    [self showUpgradeInfoBars];
  }
}

- (void)resetForTests {
  [[UpgradeCenter sharedInstance] hideUpgradeInfoBars];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kIOSChromeNextVersionKey];
  [defaults removeObjectForKey:kIOSChromeUpgradeURLKey];
  [defaults removeObjectForKey:kLastInfobarDisplayTimeKey];
  [defaults removeObjectForKey:kIOSChromeUpToDateKey];
  [_clients removeAllObjects];
}

- (void)setLastDisplayToPast {
  const base::Time pastDate =
      base::Time::Now() - kInfobarDisplayInterval - base::Seconds(1);

  [[NSUserDefaults standardUserDefaults] setObject:pastDate.ToNSDate()
                                            forKey:kLastInfobarDisplayTimeKey];
}

@end
