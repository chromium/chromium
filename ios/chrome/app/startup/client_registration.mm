// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/client_registration.h"

#import "ios/chrome/browser/net/model/chrome_cookie_store_ios_client.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/net/cookies/cookie_store_ios_client.h"
#import "ios/web/public/web_client.h"

@implementation ClientRegistration

+ (void)registerClients {
  web::SetWebClient(new ChromeWebClient());
  // Register CookieStoreIOSClient, This is used to provide CookieStoreIOSClient
  // users with WEB::IO task runner.
  net::SetCookieStoreIOSClient(new ChromeCookieStoreIOSClient());
}

@end
