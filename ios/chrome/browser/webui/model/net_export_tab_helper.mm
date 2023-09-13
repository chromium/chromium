// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/model/net_export_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper_delegate.h"
#import "ios/web/public/web_state.h"

NetExportTabHelper::NetExportTabHelper(web::WebState*) {}

NetExportTabHelper::~NetExportTabHelper() = default;

void NetExportTabHelper::ShowMailComposer(ShowMailComposerContext* context) {
  [delegate_ netExportTabHelper:this showMailComposerWithContext:context];
}

void NetExportTabHelper::SetDelegate(id<NetExportTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

WEB_STATE_USER_DATA_KEY_IMPL(NetExportTabHelper)
