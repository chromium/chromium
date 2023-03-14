// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_restore_helper.h"

#import <memory>
#import <utility>

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sessions/ios/ios_restore_live_tab.h"
#import "components/strings/grit/components_chromium_strings.h"
#import "components/strings/grit/components_google_chrome_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/browser/infobars/confirm_infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/models/image_model.h"
#import "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@protocol InfoBarManagerObserverBridgeProtocol
- (void)infoBarRemoved:(infobars::InfoBar*)infobar;
@end

// Private methods.
@interface CrashRestoreHelper ()<InfoBarManagerObserverBridgeProtocol>

// Returns a list of IDs for all backed up sessions.
+ (NSArray<NSString*>*)backedupSessionIDsForBrowserState:
    (ChromeBrowserState*)browserState;

// Restores the sessions after a crash. It should only be called if
// `moveAsideSessions:forBrowserState` for the browser state of the current
// browser was successful.
- (BOOL)restoreSessionsAfterCrash;

// The Browser instance associated with this crash restore helper.
@property(nonatomic) Browser* browser;

@end

namespace {

// The size of the symbol image.
const CGFloat kSymbolImagePointSize = 18;

// The name for directory which contains all session backup subdirectories for
// multiple sessions.
const base::FilePath::CharType kSessionBackupDirectory[] =
    FILE_PATH_LITERAL("Backups");

// The session file name on disk.
const base::FilePath::CharType kSessionBackupFileName[] =
    FILE_PATH_LITERAL("session.backup.plist");

// Convert `path` to NSString.
NSString* PathAsNSString(const base::FilePath& path) {
  return base::SysUTF8ToNSString(path.AsUTF8Unsafe());
}

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
  // Creates a session crashed infobar  and adds it to `infobar_manager`.
  static bool Create(infobars::InfoBarManager* infobar_manager,
                     CrashRestoreHelper* crash_restore_helper);

  SessionCrashedInfoBarDelegate(const SessionCrashedInfoBarDelegate&) = delete;
  SessionCrashedInfoBarDelegate& operator=(
      const SessionCrashedInfoBarDelegate&) = delete;

 private:
  SessionCrashedInfoBarDelegate(CrashRestoreHelper* crash_restore_helper);
  ~SessionCrashedInfoBarDelegate() override;

  // InfoBarDelegate:
  InfoBarIdentifier GetIdentifier() const override;

  // ConfirmInfoBarDelegate:
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;
  bool ShouldExpire(const NavigationDetails& details) const override;

  ui::ImageModel GetIcon() const override {
    if (icon_.IsEmpty()) {
      UIImage* image =
          DefaultSymbolWithPointSize(kWarningFillSymbol, kSymbolImagePointSize);
      icon_ = gfx::Image(image);
    }
    return ui::ImageModel::FromImage(icon_);
  }

  // The icon to display.
  mutable gfx::Image icon_;
  // TimeInterval when the delegate was created.
  NSTimeInterval delegate_creation_time_;
  // The CrashRestoreHelper to restore sessions.
  CrashRestoreHelper* crash_restore_helper_;
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

  std::unique_ptr<infobars::InfoBar> infobar =
      ::CreateHighPriorityConfirmInfoBar(std::move(delegate));
  return !!infobar_manager->AddInfoBar(std::move(infobar));
}

infobars::InfoBarDelegate::InfoBarIdentifier
SessionCrashedInfoBarDelegate::GetIdentifier() const {
  return SESSION_CRASHED_INFOBAR_DELEGATE_IOS;
}

std::u16string SessionCrashedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_MESSAGE);
}

int SessionCrashedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string SessionCrashedInfoBarDelegate::GetButtonLabel(
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
  // Since `restoreSessionAfterCrash` returns YES if a single NTP tab is closed,
  // which will dismiss the infobar, invert the bool.
  return ![crash_restore_helper_ restoreSessionsAfterCrash];
}

void SessionCrashedInfoBarDelegate::InfoBarDismissed() {
  base::RecordAction(base::UserMetricsAction("SessionCrashedInfobarClose"));
  [ConfirmInfobarMetricsRecorder
      recordConfirmInfobarEvent:MobileMessagesConfirmInfobarEvents::Dismissed
          forInfobarConfirmType:InfobarConfirmType::kInfobarConfirmTypeRestore];
}

bool SessionCrashedInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

}  // namespace

@implementation CrashRestoreHelper {
  Browser* _browser;
  // Indicate that the session has been restored to tabs or to recently closed
  // and should not be re-restored.
  BOOL _sessionRestored;
  std::unique_ptr<InfoBarManagerObserverBridge> _infoBarBridge;
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

+ (BOOL)deleteSessions:(NSSet<NSString*>*)sessionIDs
       forBrowserState:(ChromeBrowserState*)browserState
          shouldBackup:(BOOL)shouldBackup {
  BOOL partialSuccess = NO;
  const base::FilePath& stashPath = browserState->GetStatePath();

  for (NSString* sessionID in sessionIDs) {
    NSString* sessionPath =
        [SessionServiceIOS sessionPathForSessionID:sessionID
                                         directory:stashPath];
    NSString* backupPath = nil;
    if (shouldBackup) {
      backupPath = [self backupPathForSessionID:sessionID directory:stashPath];
    }

    partialSuccess |= [self deleteSessionFromPath:sessionPath
                                       backupFile:backupPath];
  }
  return partialSuccess;
}

+ (BOOL)deleteSessionFromPath:(NSString*)sessionPath
                   backupFile:(NSString*)backupPath {
  NSFileManager* fileManager = [NSFileManager defaultManager];
  if (![fileManager fileExistsAtPath:sessionPath])
    return NO;
  if (backupPath) {
    NSError* error = nil;
    BOOL fileOperationSuccess = [fileManager removeItemAtPath:backupPath
                                                        error:&error];
    NSInteger errorCode = fileOperationSuccess ? 0 : [error code];
    base::UmaHistogramSparse("TabRestore.error_remove_backup_at_path",
                             errorCode);
    if (!fileOperationSuccess && errorCode != NSFileNoSuchFileError) {
      return NO;
    }
    // Create the backup directory, if it doesn't exist.
    NSString* directory = [backupPath stringByDeletingLastPathComponent];
    [fileManager createDirectoryAtPath:directory
           withIntermediateDirectories:YES
                            attributes:nil
                                 error:&error];

    fileOperationSuccess = [fileManager moveItemAtPath:sessionPath
                                                toPath:backupPath
                                                 error:&error];
    errorCode = fileOperationSuccess ? 0 : [error code];
    base::UmaHistogramSparse("TabRestore.error_move_session_at_path_to_backup",
                             errorCode);
    if (!fileOperationSuccess) {
      return NO;
    }
  } else {
    NSError* error;
    BOOL fileOperationSuccess = [fileManager removeItemAtPath:sessionPath
                                                        error:&error];
    NSInteger errorCode = fileOperationSuccess ? 0 : [error code];
    base::UmaHistogramSparse("TabRestore.error_remove_session_at_path",
                             errorCode);
    if (!fileOperationSuccess) {
      return NO;
    }
  }
  return YES;
}

+ (NSString*)backupPathForSessionID:(NSString*)sessionID
                          directory:(const base::FilePath&)directory {
  return PathAsNSString(directory.Append(kSessionBackupDirectory)
                            .Append(base::SysNSStringToUTF8(sessionID))
                            .Append(kSessionBackupFileName));
}

+ (NSArray<NSString*>*)backedupSessionIDsForBrowserState:
    (ChromeBrowserState*)browserState {
  const base::FilePath backupDirectory =
      browserState->GetStatePath().Append(kSessionBackupDirectory);
  return [[NSFileManager defaultManager]
      contentsOfDirectoryAtPath:PathAsNSString(backupDirectory)
                          error:nil];
}

+ (BOOL)isBackedUpSessionID:(NSString*)sessionID
               browserState:(ChromeBrowserState*)browserState {
  return [[self backedupSessionIDsForBrowserState:browserState]
      containsObject:sessionID];
}

+ (BOOL)moveAsideSessions:(NSSet<NSString*>*)sessionIDs
          forBrowserState:(ChromeBrowserState*)browserState {
  // This may be the first time that the OTR browser state is being accessed, so
  // ensure that the OTR ChromeBrowserState is created first.
  ChromeBrowserState* otrBrowserState =
      browserState->GetOffTheRecordChromeBrowserState();
  [self deleteSessions:sessionIDs
       forBrowserState:otrBrowserState
          shouldBackup:NO];

  return [self deleteSessions:sessionIDs
              forBrowserState:browserState
                 shouldBackup:YES];
}

- (BOOL)restoreSessionsAfterCrash {
  DCHECK(!_sessionRestored);
  _sessionRestored = YES;

  // Deleting _infoBarBridge will release the owning reference it has to self
  // which may be the last reference existing. Thus it is unsafe to access to
  // the current instance after _infoBarBridge.reset(). Use a local variable
  // with precise lifetime to ensure the code self is valid till the end of the
  // current method.
  // TODO(crbug.com/1168480): fix ownership of CrashRestoreHelper.
  __attribute__((objc_precise_lifetime)) CrashRestoreHelper* keepAlive = self;
  _infoBarBridge.reset();

  return [CrashRestoreHelper
      restoreSessionsAfterCrashForBrowserState:_browser->GetBrowserState()];
}

+ (BOOL)restoreSessionsAfterCrashForBrowserState:
    (ChromeBrowserState*)browserState {
  const base::FilePath& stashPath = browserState->GetStatePath();

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);
  crash_helper::WillStartCrashRestoration();
  BOOL success = NO;
  // First restore all conected sessions.
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;

  std::set<Browser*> regularBrowsers = browserList->AllRegularBrowsers();
  for (Browser* browser : regularBrowsers) {
    NSString* sessionID =
        SessionRestorationBrowserAgent::FromBrowser(browser)->GetSessionID();

    NSString* backupPath =
        [CrashRestoreHelper backupPathForSessionID:sessionID
                                         directory:stashPath];

    SessionIOS* session =
        [[SessionServiceIOS sharedService] loadSessionFromPath:backupPath];

    if (!session)
      continue;
    success |= SessionRestorationBrowserAgent::FromBrowser(browser)
                   ->RestoreSessionWindow(session.sessionWindows[0]);

    // Remove the backup directory for this session as it will not be moved
    // back to its original browser state directory.
    [fileManager removeItemAtPath:[backupPath stringByDeletingLastPathComponent]
                            error:&error];
  }

  // Now put non restored sessions files to its original location in the browser
  // state directory.
  NSArray<NSString*>* backedupSessionIDs =
      [CrashRestoreHelper backedupSessionIDsForBrowserState:browserState];
  for (NSString* sessionID in backedupSessionIDs) {
    NSString* originalSessionPath =
        [SessionServiceIOS sessionPathForSessionID:sessionID
                                         directory:stashPath];

    NSString* backupPath =
        [CrashRestoreHelper backupPathForSessionID:sessionID
                                         directory:stashPath];

    [fileManager moveItemAtPath:backupPath
                         toPath:originalSessionPath
                          error:&error];

    // Remove Parent directory for the backup path, so it doesn't show restore
    // prompt again.
    [fileManager removeItemAtPath:[backupPath stringByDeletingLastPathComponent]
                            error:&error];
  }

  return success;
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

  ChromeBrowserState* browserState = _browser->GetBrowserState();
  const base::FilePath& stashPath = browserState->GetStatePath();

  NSArray<NSString*>* sessionsIDs =
      [CrashRestoreHelper backedupSessionIDsForBrowserState:browserState];
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSError* error = nil;
  for (NSString* sessionID in sessionsIDs) {
    NSString* backupPath =
        [CrashRestoreHelper backupPathForSessionID:sessionID
                                         directory:stashPath];

    SessionIOS* session =
        [[SessionServiceIOS sharedService] loadSessionFromPath:backupPath];

    NSArray<CRWSessionStorage*>* sessions = session.sessionWindows[0].sessions;
    if (!sessions.count)
      continue;

    sessions::TabRestoreService* const tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(browserState);
    tabRestoreService->LoadTabsFromLastSession();

    web::WebState::CreateParams params(browserState);
    for (CRWSessionStorage* session_storage in sessions) {
      auto live_tab =
          std::make_unique<sessions::RestoreIOSLiveTab>(session_storage);
      // Add all tabs at the 0 position as the position is relative to an old
      // webStateList.
      tabRestoreService->CreateHistoricalTab(live_tab.get(), 0);
    }
    [fileManager removeItemAtPath:[backupPath stringByDeletingLastPathComponent]
                            error:&error];
  }
}

@end
