// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_

#include "components/omnibox/browser/autocomplete_scheme_classifier.h"

// AutocompleteSchemeClassifierImpl provides iOS-specific implementation of
// AutocompleteSchemeClassifier interface.
class AutocompleteSchemeClassifierImpl : public AutocompleteSchemeClassifier {
 public:
  AutocompleteSchemeClassifierImpl();

  AutocompleteSchemeClassifierImpl(const AutocompleteSchemeClassifierImpl&) =
      delete;
  AutocompleteSchemeClassifierImpl& operator=(
      const AutocompleteSchemeClassifierImpl&) = delete;

  ~AutocompleteSchemeClassifierImpl() override;

  // AutocompleteInputSchemeChecker implementation.
  metrics::OmniboxInputType GetInputTypeForScheme(
      const std::string& scheme) const override;
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_AUTOCOMPLETE_SCHEME_CLASSIFIER_IMPL_H_
