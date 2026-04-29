// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"

#import "ios/web/public/web_state.h"

SendTabToSelfLoadNavigationUserData::SendTabToSelfLoadNavigationUserData(
    web::WebState* web_state,
    const std::string& entry_guid)
    : entry_guid_(entry_guid) {}

SendTabToSelfLoadNavigationUserData::~SendTabToSelfLoadNavigationUserData() =
    default;
