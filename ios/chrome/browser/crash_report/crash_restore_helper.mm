// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_restore_helper.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/ios/ios_restore_live_tab.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/infobars/confirm_infobar_controller.h"
#include "ios/chrome/browser/infobars/confirm_infobar_metrics_recorder.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol InfoBarManagerObserverBridgeProtocol
- (void)infoBarRemoved:(infobars::InfoBar*)infobar;
@end

// Private methods.
@interface CrashRestoreHelper ()<InfoBarManagerObserverBridgeProtocol>
// Deletes the session file for the given browser state, optionally backing it
// up beforehand to |backupFile| if it is not nil.  This method returns YES in
// case of success, NO otherwise.
+ (BOOL)deleteSessionForBrowserState:(ChromeBrowserState*)browserState
                          backupFile:(NSString*)file;
// Returns the path where the sessions for the main browser state are backed up.
+ (NSString*)sessionBackupPath;
// Restores the sessions after a crash. It should only be called if
// |moveAsideSessionInformation| for the browser state of the current browser
// was successful.
- (BOOL)restoreSessionsAfterCrash;
@end

namespace {

class InfoBarManagerObserverBridge : infobars::InfoBarManager::Observer {
 public:
  InfoBarManagerObserverBridge(infobars::InfoBarManager* infoBarManager,
                               id<InfoBarManagerObserverBridgeProtocol> owner)
      : infobars::InfoBarManager::Observer(),
        manager_(infoBarManager),
        owner_(owner) {
    DCHECK(infoBarManager);
    DCHECK(owner);
    manager_->AddObserver(this);
  }

  ~InfoBarManagerObserverBridge() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override {
    [owner_ infoBarRemoved:infobar];
  }

  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override {
    [owner_ infoBarRemoved:old_infobar];
  }

  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

 private:
  infobars::InfoBarManager* manager_;
  __weak id<InfoBarManagerObserverBridgeProtocol> owner_;
};

// SessionCrashedInfoBarDelegate ----------------------------------------------

// A delegate for the InfoBar shown when the previous session has crashed.
class SessionCrashedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a session crashed infobar  and adds it to |infobar_manager|.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     CrashRestoreHelper* crash_restore_helper);

 private:
  SessionCrashedInfoBarDelegate(CrashRestoreHelper* crash_restore_helper);
  ~SessionCrashedInfoBarDelegate() override;

  // InfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;
  int GetIconId() const override;

  // TimeInterval when the delegate was created.
  NSTimeInterval delegate_creation_time_;

  // The CrashRestoreHelper to restore sessions.
  CrashRestoreHelper* crash_restore_helper_;

  DISALLOW_COPY_AND_ASSIGN(SessionCrashedInfoBarDelegate);
};

SessionCrashedInfoBarDelegate::SessionCrashedInfoBarDelegate(
    CrashRestoreHelper* crash_restore_helper)
    : crash_restore_helper_(crash_restore_helper) {
  delegate_creation_time_ = [NSDate timeIntervalSinceReferenceDate];
}

SessionCrashedInfoBarDelegate::~SessionCrashedInfoBarDelegate() {}

// static
bool SessionCrashedInfoBarDelegate::Create(
    infobars::InfoBarManager* infobar_manager,
    CrashRestoreHelper* crash_restore_helper) {
  DCHECK(infobar_manager);
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new SessionCrashedInfoBarDelegate(crash_restore_helper));

  std::unique_ptr<infobars::InfoBar> infobar;
  if (IsCrashRestoreInfobarMessagesUIEnabled()) {
    infobar = ::CreateHighPriorityConfirmInfoBar(std::move(delegate));
  } else {
    ConfirmInfoBarController* controller = [[ConfirmInfoBarController alloc]
        initWithInfoBarDelegate:delegate.get()];
    infobar = std::make_unique<InfoBarIOS>(controller, std::move(delegate));
  }
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

infobars::InfoBarDelegate::InfoBarIdentifier
SessionCrashedInfoBarDelegate::GetIdentifier() const {
  return SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
}

base::string16 SessionCrashedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE);
}

int SessionCrashedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 SessionCrashedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON);
}

bool SessionCrashedInfoBarDelegate::Accept() {
  base::RecordAction(base::UserMetricsAction("SessionCrashedInfobarRestore"));
  NSTimeInterval duration =
      [NSDate timeIntervalSinceReferenceDate] - delegate_creation_time_;
  [ConfirmInfobarMetricsRecorder
      recordConfirmAcceptTime:duration
        forInfobarConfirmType:InfobarConfirmType::kInfobarConfirmTypeRestore];
  [ConfirmInfobarMetricsRecorder
      recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Accepted
          forInfobarConfirmType:InfobarConfirmType::kInfobarConfirmTypeRestore];
  // Accept should return NO if the infobar is going to be dismissed.
  // Since |restoreSessionAfterCrash| returns YES if a single NTP tab is closed,
  // which will dismiss the infobar, invert the bool.
  return ![crash_restore_helper_ restoreSessionsAfterCrash];
}

void SessionCrashedInfoBarDelegate::InfoBarDismissed() {
  base::RecordAction(base::UserMetricsAction("SessionCrashedInfobarClose"));
  [ConfirmInfobarMetricsRecorder
      recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Dismissed
          forInfobarConfirmType:InfobarConfirmType::kInfobarConfirmTypeRestore];
}

int SessionCrashedInfoBarDelegate::GetIconId() const {
  return IDR_IOS_INFOBAR_RESTORE_SESSION;
}

}  // namespace

@implementation CrashRestoreHelper {
  Browser* _browser;
  std::unique_ptr<InfoBarManagerObserverBridge> _infoBarBridge;

  // Indicate that the session has been restored to tabs or to recently closed
  // and should not be rerestored.
  BOOL _sessionRestored;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  if (self = [super init]) {
    _browser = browser;
  }
  return self;
}

- (void)showRestorePrompt {
  // Get the active webState to show the infobar on it.
  web::WebState* webState = _browser->GetWebStateList()->GetActiveWebState();
  // The last session didn't exit cleanly. Show an infobar to the user so
  // that they can restore if they want. The delegate deletes itself when
  // it is closed.
  DCHECK(webState);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  SessionCrashedInfoBarDelegate::Create(infoBarManager, self);
  [ConfirmInfobarMetricsRecorder
      recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Presented
          forInfobarConfirmType:InfobarConfirmType::kInfobarConfirmTypeRestore];
  _infoBarBridge.reset(new InfoBarManagerObserverBridge(infoBarManager, self));
}

+ (BOOL)deleteSessionForBrowserState:(ChromeBrowserState*)browserState
                          backupFile:(NSString*)file {
  NSString* stashPath =
      base::SysUTF8ToNSString(browserState->GetStatePath().value());
  NSString* sessionPath = [SessionServiceIOS sessionPathForDirectory:stashPath];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:sessionPath])
    return NO;
  if (file) {
    NSError* error = nil;
    BOOL fileOperationSuccess =
        [fileManager removeItemAtPath:file error:&error];
    NSInteger errorCode = fileOperationSuccess ? 0 : [error code];
    base::UmaHistogramSparse("TabRestore.error_remove_backup_at_path",
                             errorCode);
    if (!fileOperationSuccess && errorCode != NSFileNoSuchFileError) {
      return NO;
    }
    fileOperationSuccess =
        [fileManager moveItemAtPath:sessionPath toPath:file error:&error];
    errorCode = fileOperationSuccess ? 0 : [error code];
    base::UmaHistogramSparse("TabRestore.error_move_session_at_path_to_backup",
                             errorCode);
    if (!fileOperationSuccess) {
      return NO;
    }
  } else {
    NSError* error;
    BOOL fileOperationSuccess =
        [fileManager removeItemAtPath:sessionPath error:&error];
    NSInteger errorCode = fileOperationSuccess ? 0 : [error code];
    base::UmaHistogramSparse("TabRestore.error_remove_session_at_path",
                             errorCode);
    if (!fileOperationSuccess) {
      return NO;
    }
  }
  return YES;
}

+ (NSString*)sessionBackupPath {
  NSString* tmpDirectory = NSTemporaryDirectory();
  return [tmpDirectory stringByAppendingPathComponent:@"session.bak"];
}

+ (BOOL)moveAsideSessionInformationForBrowserState:
    (ChromeBrowserState*)browserState {
  // This may be the first time that the OTR browser state is being accessed, so
  // ensure that the OTR ChromeBrowserState is created first.
  ChromeBrowserState* otrBrowserState =
      browserState->GetOffTheRecordChromeBrowserState();
  [self deleteSessionForBrowserState:otrBrowserState backupFile:nil];
  return [self deleteSessionForBrowserState:browserState
                                 backupFile:[self sessionBackupPath]];
}

- (BOOL)restoreSessionsAfterCrash {
  DCHECK(!_sessionRestored);
  _sessionRestored = YES;
  _infoBarBridge.reset();

  SessionIOS* session = [[SessionServiceIOS sharedService]
      loadSessionFromPath:[[self class] sessionBackupPath]];
  if (!session)
    return NO;

  DCHECK_EQ(session.sessionWindows.count, 1u);
  breakpad_helper::WillStartCrashRestoration();
  return SessionRestorationBrowserAgent::FromBrowser(_browser)
      ->RestoreSessionWindow(session.sessionWindows[0]);
}

- (void)infoBarRemoved:(infobars::InfoBar*)infobar {
  DCHECK(infobar->delegate());
  if (_sessionRestored ||
      infobar->delegate()->GetIdentifier() !=
          infobars::InfoBarDelegate::SESSION_CRASHED_INFOBAR_DELEGATE_IOS) {
    return;
  }

  // If the infobar is dismissed without restoring the tabs (either by closing
  // it with the cross or after a navigation), all the entries will be added to
  // the recently closed tabs.
  _sessionRestored = YES;

  SessionIOS* session = [[SessionServiceIOS sharedService]
      loadSessionFromPath:[[self class] sessionBackupPath]];
  DCHECK_EQ(session.sessionWindows.count, 1u);

  NSArray<CRWSessionStorage*>* sessions = session.sessionWindows[0].sessions;
  if (!sessions.count)
    return;

  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          _browser->GetBrowserState());
  tabRestoreService->LoadTabsFromLastSession();

  web::WebState::CreateParams params(_browser->GetBrowserState());
  for (CRWSessionStorage* session in sessions) {
    auto live_tab = std::make_unique<sessions::RestoreIOSLiveTab>(session);
    // Add all tabs at the 0 position as the position is relative to an old
    // tabModel.
    tabRestoreService->CreateHistoricalTab(live_tab.get(), 0);
  }
  return;
}

@end
