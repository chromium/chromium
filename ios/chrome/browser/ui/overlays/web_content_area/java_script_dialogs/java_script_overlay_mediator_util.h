// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_OVERLAY_MEDIATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_OVERLAY_MEDIATOR_UTIL_H_

#import <Foundation/Foundation.h>
#include <string>

class JavaScriptDialogSource;

NSString* GetJavaScriptDialogTitle(const JavaScriptDialogSource& source,
                                   const std::string& config_message);

NSString* GetJavaScriptDialogMessage(const JavaScriptDialogSource& source,
                                     const std::string& config_message);

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_JAVA_SCRIPT_DIALOGS_JAVA_SCRIPT_OVERLAY_MEDIATOR_UTIL_H_
