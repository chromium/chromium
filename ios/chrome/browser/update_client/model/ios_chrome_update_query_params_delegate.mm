// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/update_client/model/ios_chrome_update_query_params_delegate.h"

#import "base/no_destructor.h"
#import "base/strings/strcat.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/common/channel_info.h"

IOSChromeUpdateQueryParamsDelegate::IOSChromeUpdateQueryParamsDelegate() {}

IOSChromeUpdateQueryParamsDelegate::~IOSChromeUpdateQueryParamsDelegate() {}

// static
IOSChromeUpdateQueryParamsDelegate*
IOSChromeUpdateQueryParamsDelegate::GetInstance() {
  static base::NoDestructor<IOSChromeUpdateQueryParamsDelegate> instance;
  return instance.get();
}

std::string IOSChromeUpdateQueryParamsDelegate::GetExtraParams() {
  return base::StrCat({"&prodchannel=", GetChannelString(), "&prodversion=",
                       version_info::GetVersionNumber(), "&lang=", GetLang()});
}

// static
const std::string& IOSChromeUpdateQueryParamsDelegate::GetLang() {
  return GetApplicationContext()->GetApplicationLocale();
}
