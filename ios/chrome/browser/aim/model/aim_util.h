// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AIM_UTIL_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AIM_UTIL_H_

#import <Foundation/Foundation.h>

namespace lens {
enum class LensOverlayInvocationSource;
}  // namespace lens

namespace omnibox {
enum ChromeAimEntryPoint : int;
}  // namespace omnibox

class TemplateURLService;
class UrlLoadingBrowserAgent;

// Opens the AIM search results page in the current tab using the specified
// entry point and invocation source.
void OpenAimInCurrentTab(TemplateURLService* template_url_service,
                         UrlLoadingBrowserAgent* url_loader,
                         omnibox::ChromeAimEntryPoint entry_point,
                         lens::LensOverlayInvocationSource invocation_source,
                         BOOL in_incognito);

// Opens the AIM search results page in the current tab for the App Bar entry
// point.
void OpenAppBarAimInCurrentTab(TemplateURLService* template_url_service,
                               UrlLoadingBrowserAgent* url_loader,
                               BOOL in_incognito);

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AIM_UTIL_H_
