// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_UTIL_H_

class PrefService;
class TemplateURLService;

namespace search_engines {

// Updates the current country code pref for the search engine.
void UpdateSearchEngineCountryCodeIfNeeded(PrefService* preferences);

// Checks whether the default url of the given template url supports searching
// by image.
bool SupportsSearchByImage(TemplateURLService* service);

// Checks whether the default url of the given template url supports searching
// an image with Google Lens.
bool SupportsSearchImageWithLens(TemplateURLService* service);

}  // namespace search_engines

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_UTIL_H_
