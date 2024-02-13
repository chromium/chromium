// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_UTILS_UI_UTILS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_UTILS_UI_UTILS_API_H_

#import <UIKit/UIKit.h>

class Browser;
@protocol LogoVendor;

namespace web {
class WebState;
}

namespace ios {
namespace provider {

// Initializes UI global state for the provider.
void InitializeUI();

// Creates a new LogoVendor instance.
id<LogoVendor> CreateLogoVendor(Browser* browser, web::WebState* web_state);

// Hides immediately the modals related to this provider.
void HideModalViewStack();

// Logs if any modals created by this provider are still presented. It does
// not dismiss them.
void LogIfModalViewsArePresented();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_UTILS_UI_UTILS_API_H_
