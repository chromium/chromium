// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import "base/check_op.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/crw_data_controls_delegate.h"
#import "ios/web/web_state/crw_web_view.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {

WKWebView* BuildWKWebViewForQueries(WKWebViewConfiguration* configuration,
                                    BrowserState* browser_state) {
  CHECK(browser_state);
  CHECK(configuration);
  return [[WKWebView alloc] initWithFrame:CGRectZero
                            configuration:configuration];
}

CRWWebView* BuildWKWebView(CGRect frame,
                           WKWebViewConfiguration* configuration,
                           BrowserState* browser_state,
                           UserAgentType user_agent_type,
                           id<CRWInputViewProvider> input_view_provider,
                           id<CRWEditMenuBuilder> edit_menu_builder,
                           id<CRWDataControlsDelegate> data_controls_delegate) {
  CHECK(browser_state);
  CHECK(configuration);

  GetWebClient()->PreWebViewCreation();

  CRWWebView* web_view = [[CRWWebView alloc] initWithFrame:frame
                                             configuration:configuration];
  web_view.inputViewProvider = input_view_provider;
  web_view.editMenuBuilder = edit_menu_builder;
  web_view.dataControlsDelegate = data_controls_delegate;

  // Set the user agent type.
  if (user_agent_type != web::UserAgentType::NONE) {
    web_view.customUserAgent = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(user_agent_type));
  }

  if (@available(iOS 16.4, *)) {
    if (web::GetWebClient()->EnableWebInspector(browser_state)) {
      web_view.inspectable = YES;
    }
  }

  return web_view;
}

}  // namespace web
