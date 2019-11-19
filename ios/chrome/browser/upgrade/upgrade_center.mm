// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_center.h"

#include <memory>
#include <set>
#include <utility>

#include "base/mac/bundle_locations.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/version_info/version_info.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
// The dispatcher for this object.
@property(nonatomic, weak) id<ApplicationCommands> dispatcher;
@end

namespace {

// The user defaults key for the upgrade version.
NSString* const kNextVersionKey = @"UpdateInfobarNextVersion";
// The user defaults key for the upgrade URL.
NSString* const kUpgradeURLKey = @"UpdateInfobarUpgradeURL";
// The user defaults key for the last time the update infobar was shown.
NSString* const kLastInfobarDisplayTimeKey = @"UpdateInfobarLastDisplayTime";
// The amount of time that must elapse before showing the infobar again.
const NSTimeInterval kInfobarDisplayInterval = 24 * 60 * 60;  // One day.

// The class controlling the look of the infobar displayed when an upgrade is
// available.
class UpgradeInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  UpgradeInfoBarDelegate() : trigger_upgrade_(false) {}

  ~UpgradeInfoBarDelegate() override {}

  // Returns true is the infobar was closed by pressing the accept button.
  bool AcceptPressed() { return trigger_upgrade_; }

  void RemoveSelf() {
    infobars::InfoBar* infobar = this->infobar();
    if (infobar)
      infobar->RemoveSelf();
  }

 private:
  InfoBarIdentifier GetIdentifier() const override {
    return UPGRADE_INFOBAR_DELEGATE_IOS;
  }

  bool ShouldExpire(const NavigationDetails& details) const override {
    return false;
  }

  gfx::Image GetIcon() const override {
    if (icon_.IsEmpty()) {
      icon_ = gfx::Image([UIImage imageNamed:@"infobar_update"]);
    }
    return icon_;
  }

  base::string16 GetMessageText() const override {
    return l10n_util::GetStringUTF16(IDS_IOS_UPGRADE_AVAILABLE);
  }

  bool Accept() override {
    trigger_upgrade_ = true;
    return true;
  }

  int GetButtons() const override { return BUTTON_OK; }

  base::string16 GetButtonLabel(InfoBarButton button) const override {
    DCHECK(button == BUTTON_OK);
    return l10n_util::GetStringUTF16(IDS_IOS_UPGRADE_AVAILABLE_BUTTON);
  }

  mutable gfx::Image icon_;
  bool trigger_upgrade_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeInfoBarDelegate);
};

// The InfoBarDelegate unfortunately is not called at all when an infoBar is
// simply dismissed. In order to catch that case this object listens to the
// infobars::InfoBarManager::Observer::OnInfoBarRemoved() which is invoked when
// an infobar is closed, for any reason.
class UpgradeInfoBarDismissObserver
    : public infobars::InfoBarManager::Observer {
 public:
  UpgradeInfoBarDismissObserver()
      : infobar_delegate_(nullptr),
        dismiss_delegate_(nil),
        scoped_observer_(this) {}

  ~UpgradeInfoBarDismissObserver() override {}

  void RegisterObserver(infobars::InfoBarManager* infobar_manager,
                        UpgradeInfoBarDelegate* infobar_delegate,
                        NSString* tab_id,
                        UpgradeCenter* dismiss_delegate) {
    scoped_observer_.Add(infobar_manager);
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
    scoped_observer_.Remove(infobar_manager);
  }

  UpgradeInfoBarDelegate* infobar_delegate_;
  __weak UpgradeCenter* dismiss_delegate_;
  __strong NSString* tab_id_;
  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeInfoBarDismissObserver);
};

}  // namespace

// The delegateHolder is a simple wrapper to be able to store all the
// infoBarDelegate related information in an object that can be put in
// an ObjectiveC container.
@interface DelegateHolder : NSObject {
  UpgradeInfoBarDismissObserver observer_;
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
    observer_.RegisterObserver(infoBarManager, infoBarDelegate, tabId,
                               upgradeCenter);
  }
  return self;
}

- (UpgradeInfoBarDelegate*)infoBarDelegate {
  return observer_.infobar_delegate();
}

@end

@implementation UpgradeCenter {
  // YES if the infobars are currently visible.
  BOOL upgradeInfoBarIsVisible_;
  // Used to store the visible upgrade infobars, indexed by tabId.
  __strong NSMutableDictionary<NSString*, DelegateHolder*>*
      upgradeInfoBarDelegates_;
  // Stores the clients of the upgrade center. These objectiveC objects are not
  // retained.
  __strong NSHashTable<id<UpgradeCenterClient>>* clients_;
#if DCHECK_IS_ON()
  BOOL inCallback_;
#endif
}
@synthesize dispatcher = _dispatcher;

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
    upgradeInfoBarDelegates_ = [[NSMutableDictionary alloc] init];

    // There is no dealloc and no unregister as this class is a never
    // deallocated singleton.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationWillEnterForeground:)
               name:UIApplicationWillEnterForegroundNotification
             object:nil];

    upgradeInfoBarIsVisible_ = [self shouldShowInfoBar];
    clients_ = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (BOOL)shouldShowInfoBar {
  return [self isCurrentVersionObsolete] && ![self infoBarShownRecently];
}

- (BOOL)isCurrentVersionObsolete {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* nextVersion = [defaults stringForKey:kNextVersionKey];
  if (nextVersion) {
    const base::Version& current_version = version_info::GetVersion();
    const std::string upgrade = base::SysNSStringToUTF8(nextVersion);
    return current_version < base::Version(upgrade);
  }
  return NO;
}

- (BOOL)infoBarShownRecently {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSDate* lastDisplay = [defaults objectForKey:kLastInfobarDisplayTimeKey];
  // Absolute value is to ensure the infobar won't be suppressed forever if the
  // clock temporarily jumps to the distant future.
  if (lastDisplay &&
      fabs([lastDisplay timeIntervalSinceNow]) < kInfobarDisplayInterval) {
    return YES;
  }
  return NO;
}

- (void)applicationWillEnterForeground:(NSNotification*)note {
  if (upgradeInfoBarIsVisible_)
    return;

  // When returning to active if the upgrade notification has been dismissed,
  // bring it back.
  if ([self shouldShowInfoBar])
    [self showUpgradeInfoBars];
}

- (void)registerClient:(id<UpgradeCenterClient>)client
        withDispatcher:(id<ApplicationCommands>)dispatcher {
  [clients_ addObject:client];
  self.dispatcher = dispatcher;
  if (upgradeInfoBarIsVisible_)
    [client showUpgrade:self];
}

- (void)unregisterClient:(id<UpgradeCenterClient>)client {
#if DCHECK_IS_ON()
  DCHECK(!inCallback_);
#endif
  [clients_ removeObject:client];
}

- (void)addInfoBarToManager:(infobars::InfoBarManager*)infoBarManager
                   forTabId:(NSString*)tabId {
  DCHECK(tabId);
  DCHECK(infoBarManager);

  // Nothing to do if the infobar are not visible at this point in time.
  if (!upgradeInfoBarIsVisible_)
    return;

  // Nothing to do if the infobar is already there.
  if ([upgradeInfoBarDelegates_ objectForKey:tabId])
    return;

  auto infobarDelegate = std::make_unique<UpgradeInfoBarDelegate>();
  DelegateHolder* delegateHolder =
      [[DelegateHolder alloc] initWithInfoBarManager:infoBarManager
                                     infoBarDelegate:infobarDelegate.get()
                                       upgradeCenter:self
                                               tabId:tabId];

  [upgradeInfoBarDelegates_ setObject:delegateHolder forKey:tabId];
  infoBarManager->AddInfoBar(
      infoBarManager->CreateConfirmInfoBar(std::move(infobarDelegate)));
}

- (void)tabWillClose:(NSString*)tabId {
  [upgradeInfoBarDelegates_ removeObjectForKey:tabId];
}

- (void)dismissedInfoBar:(NSString*)tabId performUpgrade:(BOOL)shouldUpgrade {
  // If the tabId is not in the upgradeInfoBarDelegates_ just ignore the
  // notification. In all likelihood it was trigerred by calling
  // -hideUpgradeInfoBars. Or because a tab was closed without dismissing the
  // infobar.
  DelegateHolder* delegateHolder =
      [upgradeInfoBarDelegates_ objectForKey:tabId];
  if (!delegateHolder)
    return;

  // Forget about this dismissed infobar.
  [upgradeInfoBarDelegates_ removeObjectForKey:tabId];

  // Get rid of all the infobars on the other tabs.
  [self hideUpgradeInfoBars];

  if (shouldUpgrade) {
    NSString* urlString =
        [[NSUserDefaults standardUserDefaults] valueForKey:kUpgradeURLKey];
    if (!urlString)
      return;  // Missing URL, no upgrade possible.

    GURL URL = GURL(base::SysNSStringToUTF8(urlString));
    if (!URL.is_valid())
      return;

    if (web::UrlHasWebScheme(URL)) {
      // This URL can be opened in the application, just open in a new tab.
      OpenNewTabCommand* command =
          [OpenNewTabCommand commandWithURLFromChrome:URL];
      [self.dispatcher openURLInNewTab:command];
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
  inCallback_ = YES;
#endif
  upgradeInfoBarIsVisible_ = YES;
  for (id<UpgradeCenterClient> upgradeClient in clients_)
    [upgradeClient showUpgrade:self];
#if DCHECK_IS_ON()
  inCallback_ = NO;
#endif

  [[NSUserDefaults standardUserDefaults] setObject:[NSDate date]
                                            forKey:kLastInfobarDisplayTimeKey];
}

- (void)hideUpgradeInfoBars {
  upgradeInfoBarIsVisible_ = NO;
  // It is important to call -allKeys here and not using a fast iteration on the
  // dictionary directly: the dictionary is modified as we go...
  for (NSString* tabId in [upgradeInfoBarDelegates_ allKeys]) {
    // It is important to retain the delegateHolder as otherwise it is
    // deallocated as soon as it is removed from the dictionary.
    DelegateHolder* delegateHolder =
        [upgradeInfoBarDelegates_ objectForKey:tabId];
    if (delegateHolder) {
      [upgradeInfoBarDelegates_ removeObjectForKey:tabId];
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
  NSString* previousVersionString = [defaults stringForKey:kNextVersionKey];
  if (!previousVersionString ||
      ![previousVersionString isEqualToString:newVersionString]) {
    [defaults removeObjectForKey:kLastInfobarDisplayTimeKey];
  }

  [defaults setValue:base::SysUTF8ToNSString(upgradeUrl.spec())
              forKey:kUpgradeURLKey];
  [defaults setValue:newVersionString forKey:kNextVersionKey];

  if ([self shouldShowInfoBar])
    [self showUpgradeInfoBars];
}

- (void)resetForTests {
  [[UpgradeCenter sharedInstance] hideUpgradeInfoBars];
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults removeObjectForKey:kNextVersionKey];
  [defaults removeObjectForKey:kUpgradeURLKey];
  [defaults removeObjectForKey:kLastInfobarDisplayTimeKey];
  [clients_ removeAllObjects];
}

- (void)setLastDisplayToPast {
  NSDate* pastDate =
      [NSDate dateWithTimeIntervalSinceNow:-(kInfobarDisplayInterval + 1)];
  [[NSUserDefaults standardUserDefaults] setObject:pastDate
                                            forKey:kLastInfobarDisplayTimeKey];
}

@end
