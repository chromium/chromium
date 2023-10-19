// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_UTIL_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_UTIL_H_

class PrefService;
class TemplateURLService;

namespace search_engines {

// Updates the current search engine, as well as the list of available search
// engines if the locale of the user changed.
// TODO(crbug.com/153047): Once user can customize search engines remove this
// method.
void UpdateSearchEnginesIfNeeded(PrefService* preferences,
                                 TemplateURLService* service);

// Checks whether the default url of the given template url supports searching
// by image.
bool SupportsSearchByImage(TemplateURLService* service);

// Checks whether the default url of the given template url supports searching
// an image with Google Lens.
bool SupportsSearchImageWithLens(TemplateURLService* service);

}  // namespace search_engines

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINES_UTIL_H_
