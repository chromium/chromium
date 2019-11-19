// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import <objc/runtime.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/crw_context_menu_controller.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

// Verifies the preconditions for creating a WKWebView. Must be called before
// a WKWebView is allocated. Not verifying the preconditions before creating
// a WKWebView will lead to undefined behavior.
void VerifyWKWebViewCreationPreConditions(
    BrowserState* browser_state,
    WKWebViewConfiguration* configuration) {
  DCHECK(browser_state);
  DCHECK(configuration);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  DCHECK_EQ([config_provider.GetWebViewConfiguration() processPool],
            [configuration processPool]);
}

}  // namespace

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type,
                          id<CRWContextMenuDelegate> context_menu_delegate) {
  VerifyWKWebViewCreationPreConditions(browser_state, configuration);

  GetWebClient()->PreWebViewCreation();
  WKWebView* web_view =
      [[WKWebView alloc] initWithFrame:frame configuration:configuration];

  // Set the user agent type.
  if (user_agent_type != web::UserAgentType::NONE) {
    web_view.customUserAgent = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(user_agent_type));
  }

  // By default the web view uses a very sluggish scroll speed. Set it to a more
  // reasonable value.
  web_view.scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;

  if (context_menu_delegate) {
    CRWContextMenuController* context_menu_controller =
        [[CRWContextMenuController alloc]
            initWithWebView:web_view
               browserState:browser_state
                   delegate:context_menu_delegate];
    void* associated_object_key = (__bridge void*)context_menu_controller;
    objc_setAssociatedObject(web_view, associated_object_key,
                             context_menu_controller,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }

  // Uses the default value for |allowsLinkPreview| i.e., YES in iOS 10 or
  // later, and NO for iOS 9 or before. But the link preview is still disabled
  // by default on iOS 10 or later. You need to return true from
  // web::WebStateDelegate::ShouldPreviewLink() to enable the preview.
  return web_view;
}

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type) {
  return BuildWKWebView(frame, configuration, browser_state, user_agent_type,
                        nil);
}

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state) {
  return BuildWKWebView(frame, configuration, browser_state,
                        UserAgentType::MOBILE);
}

}  // namespace web
