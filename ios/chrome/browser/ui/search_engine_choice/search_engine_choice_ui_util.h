// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_

#import <UIKit/UIKit.h>

class TemplateURL;

// UI Util containing helper methods for the choice screen UI.

// Returns embedded favicon for search engine from `template_url`. The search
// engine has to be prepopulated.
UIImage* SearchEngineFaviconFromTemplateURL(const TemplateURL& template_url);

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_
