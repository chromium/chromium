// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook_helper.h"

#import "base/strings/stringprintf.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"

void InjectUnrealizedWebStatesUntilListHasSizeItems(Browser* browser,
                                                    int count) {
  WebStateList* web_state_list = browser->GetWebStateList();
  count -= web_state_list->count();

  if (count <= 0) {
    return;
  }

  SessionRestorationService* service =
      SessionRestorationServiceFactory::GetForProfile(browser->GetProfile());

  auto scoped_lock = web_state_list->StartBatchOperation();
  for (int i = 0; i < count; ++i) {
    std::string string_url = base::StringPrintf("http://google.com/%d", i);

    // Create the serialized representation of a WebState
    // with one navigation to `string_url` (defaulting the
    // title to the URL).
    web::proto::WebStateStorage storage = web::CreateWebStateStorage(
        web::NavigationManager::WebLoadParams(GURL(string_url)),
        base::UTF8ToUTF16(string_url.c_str()),
        /*created_with_opener=*/false, web::UserAgentType::MOBILE,
        base::Time::Now());

    // Ask the SessionService to create an unrealized WebState
    // and to prepare itself for it to be added to `browser`.
    std::unique_ptr<web::WebState> web_state =
        service->CreateUnrealizedWebState(browser, std::move(storage));

    // Insert the new unrealized WebState in `browser`.
    // Need to activate one WebState otherwise the session
    // will not be saved with the legacy session storage.
    int index = browser->GetWebStateList()->count();
    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate(
            index == 0 && !web_state_list->GetActiveWebState()));
  }
}
