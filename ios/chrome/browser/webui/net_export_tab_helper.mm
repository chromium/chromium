// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/net_export_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NetExportTabHelper::NetExportTabHelper(web::WebState*) {}

NetExportTabHelper::~NetExportTabHelper() = default;

void NetExportTabHelper::ShowMailComposer(ShowMailComposerContext* context) {
  [delegate_ netExportTabHelper:this showMailComposerWithContext:context];
}

void NetExportTabHelper::SetDelegate(id<NetExportTabHelperDelegate> delegate) {
  delegate_ = delegate;
}

WEB_STATE_USER_DATA_KEY_IMPL(NetExportTabHelper)
