// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/power_bookmarks/model/power_bookmark_service_factory.h"

#import "base/task/thread_pool.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/power_bookmarks/core/power_bookmark_service.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

// static
power_bookmarks::PowerBookmarkService*
PowerBookmarkServiceFactory::GetForBrowserState(web::BrowserState* state) {
  return static_cast<power_bookmarks::PowerBookmarkService*>(
      GetInstance()->GetServiceForBrowserState(state, true));
}

// static
PowerBookmarkServiceFactory* PowerBookmarkServiceFactory::GetInstance() {
  return base::Singleton<PowerBookmarkServiceFactory>::get();
}

PowerBookmarkServiceFactory::PowerBookmarkServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PowerBookmarkService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::LocalOrSyncableBookmarkModelFactory::GetInstance());
}

PowerBookmarkServiceFactory::~PowerBookmarkServiceFactory() = default;

std::unique_ptr<KeyedService>
PowerBookmarkServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* state) const {
  ChromeBrowserState* chrome_state =
      ChromeBrowserState::FromBrowserState(state);
  return std::make_unique<power_bookmarks::PowerBookmarkService>(
      ios::LocalOrSyncableBookmarkModelFactory::GetInstance()
          ->GetForBrowserState(chrome_state),
      state->GetStatePath().AppendASCII("power_bookmarks"),
      web::GetUIThreadTaskRunner({}),
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));
}

web::BrowserState* PowerBookmarkServiceFactory::GetBrowserStateToUse(
    web::BrowserState* state) const {
  return GetBrowserStateRedirectedInIncognito(state);
}

bool PowerBookmarkServiceFactory::ServiceIsCreatedWithBrowserState() const {
  return true;
}

bool PowerBookmarkServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
