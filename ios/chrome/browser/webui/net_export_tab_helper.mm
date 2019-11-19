// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/net_export_tab_helper.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
void NetExportTabHelper::CreateForWebState(
    web::WebState* web_state,
    id<NetExportTabHelperDelegate> delegate) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           base::WrapUnique(new NetExportTabHelper(delegate)));
  }
}

NetExportTabHelper::NetExportTabHelper(id<NetExportTabHelperDelegate> delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

NetExportTabHelper::~NetExportTabHelper() = default;

void NetExportTabHelper::ShowMailComposer(ShowMailComposerContext* context) {
  [delegate_ netExportTabHelper:this showMailComposerWithContext:context];
}

WEB_STATE_USER_DATA_KEY_IMPL(NetExportTabHelper)
