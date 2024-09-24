// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_util.h"

#import <string_view>

#import "base/check_op.h"
#import "base/files/file_path.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/serialized_navigation_entry.h"
#import "components/sessions/ios/ios_serialized_navigation_builder.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace session_util {
namespace {

// Suffix appended to the SceneState session identifier for inactive Browsers.
constexpr std::string_view kInactiveBrowserIdentifierSuffix = "-Inactive";

}  // namespace

std::unique_ptr<web::WebState> CreateWebStateWithNavigationEntries(
    ProfileIOS* profile,
    int last_committed_item_index,
    const std::vector<sessions::SerializedNavigationEntry>& navigations) {
  DCHECK_GE(last_committed_item_index, 0);
  DCHECK_LT(static_cast<size_t>(last_committed_item_index), navigations.size());

  web::WebState::CreateParams params(profile);
  auto web_state = web::WebState::Create(params);
  web_state->GetNavigationManager()->Restore(
      last_committed_item_index,
      sessions::IOSSerializedNavigationBuilder::ToNavigationItems(navigations));
  return web_state;
}

std::string GetSessionIdentifier(Browser* browser) {
  SceneState* scene_state = browser->GetSceneState();
  NSString* scene_session = scene_state.sceneSessionID;
  DCHECK(scene_session.length);

  return GetSessionIdentifier(base::SysNSStringToUTF8(scene_session),
                              browser->IsInactive());
}

std::string GetSessionIdentifier(const std::string& scene_session_identifier,
                                 bool inactive_browser) {
  if (!inactive_browser) {
    return scene_session_identifier;
  }

  return base::StrCat(
      {scene_session_identifier, kInactiveBrowserIdentifierSuffix});
}

}  // namespace session_util
