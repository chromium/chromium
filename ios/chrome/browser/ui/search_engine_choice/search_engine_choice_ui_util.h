// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/favicon/model/favicon_loader.h"

class FaviconLoader;
class TemplateURL;
class TemplateURLService;

namespace search_engines {
class SearchEngineChoiceService;
}  // namespace search_engines

// UI Util containing helper methods for the choice screen UI.

// Returns embedded favicon for search engine from `template_url`. The search
// engine has to be prepopulated. Returns `nil` if the icon is not embedded
// in Chrome.
UIImage* SearchEngineFaviconFromTemplateURL(const TemplateURL& template_url);

// Gets the favicon for `template_url` and calls `favicon_block_handler`. The
// call can be synchronous or asynchronous depending on whether the image is
// available or not.
void GetSearchEngineFavicon(
    const TemplateURL& template_url,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    TemplateURLService* template_url_service,
    FaviconLoader* favicon_loader,
    FaviconLoader::FaviconAttributesCompletionBlock favicon_block_handler);

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_UI_UTIL_H_
