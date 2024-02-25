// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_TAB_MATCHER_IMPL_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_TAB_MATCHER_IMPL_H_

#import "base/memory/raw_ptr.h"
#include "components/omnibox/browser/tab_matcher.h"

class ChromeBrowserState;

class TabMatcherImpl : public TabMatcher {
 public:
  explicit TabMatcherImpl(ChromeBrowserState* browser_state);

  bool IsTabOpenWithURL(const GURL& gurl,
                        const AutocompleteInput* input) const override;

 private:
  raw_ptr<ChromeBrowserState> browser_state_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_TAB_MATCHER_IMPL_H_
