// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/update_client/ios_chrome_update_query_params_delegate.h"

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/common/channel_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeUpdateQueryParamsDelegate::IOSChromeUpdateQueryParamsDelegate() {}

IOSChromeUpdateQueryParamsDelegate::~IOSChromeUpdateQueryParamsDelegate() {}

// static
IOSChromeUpdateQueryParamsDelegate*
IOSChromeUpdateQueryParamsDelegate::GetInstance() {
  static base::NoDestructor<IOSChromeUpdateQueryParamsDelegate> instance;
  return instance.get();
}

std::string IOSChromeUpdateQueryParamsDelegate::GetExtraParams() {
  return base::StringPrintf(
      "&prodchannel=%s&prodversion=%s&lang=%s", GetChannelString().c_str(),
      version_info::GetVersionNumber().c_str(), GetLang().c_str());
}

// static
const std::string& IOSChromeUpdateQueryParamsDelegate::GetLang() {
  return GetApplicationContext()->GetApplicationLocale();
}
