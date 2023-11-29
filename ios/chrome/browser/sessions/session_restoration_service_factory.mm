// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_service_factory.h"

#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/legacy_session_restoration_service.h"
#import "ios/chrome/browser/sessions/session_restoration_service_impl.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/web/session_state/web_session_state_cache_factory.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

namespace {

// Value taken from Desktop Chrome.
constexpr base::TimeDelta kSaveDelay = base::Seconds(2.5);

}  // namespace

// static
SessionRestorationService* SessionRestorationServiceFactory::GetForBrowserState(
    ChromeBrowserState* browser_state) {
  return static_cast<SessionRestorationService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
SessionRestorationServiceFactory*
SessionRestorationServiceFactory::GetInstance() {
  static base::NoDestructor<SessionRestorationServiceFactory> instance;
  return instance.get();
}

SessionRestorationServiceFactory::SessionRestorationServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "SessionRestorationService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(WebSessionStateCacheFactory::GetInstance());
  DependsOn(IOSChromeTabRestoreServiceFactory::GetInstance());
}

SessionRestorationServiceFactory::~SessionRestorationServiceFactory() = default;

std::unique_ptr<KeyedService>
SessionRestorationServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ChromeBrowserState* browser_state =
      ChromeBrowserState::FromBrowserState(context);

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  const base::FilePath storage_path = browser_state->GetStatePath();

  // If the optimised session restoration format is not enabled, create a
  // LegacySessionRestorationService instance which wraps the legacy API.
  if (!web::features::UseSessionSerializationOptimizations()) {
    SessionServiceIOS* session_service_ios =
        [[SessionServiceIOS alloc] initWithSaveDelay:kSaveDelay
                                          taskRunner:task_runner];

    return std::make_unique<LegacySessionRestorationService>(
        IsPinnedTabsEnabled(), storage_path, session_service_ios,
        WebSessionStateCacheFactory::GetForBrowserState(browser_state),
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state));
  }

  return std::make_unique<SessionRestorationServiceImpl>(
      kSaveDelay, IsPinnedTabsEnabled(), storage_path, task_runner,
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state));
}

web::BrowserState* SessionRestorationServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateOwnInstanceInIncognito(context);
}
