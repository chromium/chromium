// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/ios/ios_serialized_navigation_builder.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace session_util {

std::unique_ptr<web::WebState> CreateWebStateWithNavigationEntries(
    ios::ChromeBrowserState* browser_state,
    int last_committed_item_index,
    const std::vector<sessions::SerializedNavigationEntry>& navigations) {
  DCHECK_GE(last_committed_item_index, 0);
  DCHECK_LT(static_cast<size_t>(last_committed_item_index), navigations.size());

  web::WebState::CreateParams params(browser_state);
  auto web_state = web::WebState::Create(params);
  web_state->GetNavigationManager()->Restore(
      last_committed_item_index,
      sessions::IOSSerializedNavigationBuilder::ToNavigationItems(navigations));
  return web_state;
}

}  // namespace session_util
