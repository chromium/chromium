// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_CWV_FRAMEWORK_H_
#define IOS_WEB_VIEW_SHELL_CWV_FRAMEWORK_H_

#import "ios/web_view/shell/buildflags.h"

#if BUILDFLAG(IOS_WEB_VIEW_INCLUDE_CRONET)
#import <CronetChromeWebView/CronetChromeWebView.h>
#else
#import <ChromeWebView/ChromeWebView.h>
#endif

#endif  // IOS_WEB_VIEW_SHELL_CWV_FRAMEWORK_H_
