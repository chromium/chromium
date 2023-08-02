// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_earl_grey_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/testing/nserror_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

using web::shell_test_util::GetCurrentWebState;

@implementation ShellEarlGreyAppInterface

+ (void)startLoadingURL:(NSString*)spec {
  web::test::LoadUrl(GetCurrentWebState(), GURL(base::SysNSStringToUTF8(spec)));
}

+ (BOOL)isCurrentWebStateLoading {
  return GetCurrentWebState()->IsLoading();
}

+ (BOOL)currentWebStateContainsText:(NSString*)text {
  return web::test::IsWebViewContainingText(GetCurrentWebState(),
                                            base::SysNSStringToUTF8(text));
}

@end
