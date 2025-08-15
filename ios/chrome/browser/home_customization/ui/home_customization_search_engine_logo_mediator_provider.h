// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_SEARCH_ENGINE_LOGO_MEDIATOR_PROVIDER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_SEARCH_ENGINE_LOGO_MEDIATOR_PROVIDER_H_

@class SearchEngineLogoMediator;

// A protocol for providing a logo vendor object used in Home customization.
@protocol HomeCustomizationSearchEngineLogoMediatorProvider

// Returns a SearchEngineLogoMediator for the given key.
// May return a cached instance or create a new one as needed.
- (SearchEngineLogoMediator*)provideSearchEngineLogoMediatorForKey:
    (NSString*)key;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_SEARCH_ENGINE_LOGO_MEDIATOR_PROVIDER_H_
