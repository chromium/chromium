// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ios/chrome/browser/chrome_url_util.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AutocompleteSchemeClassifierImpl::AutocompleteSchemeClassifierImpl() {}

AutocompleteSchemeClassifierImpl::~AutocompleteSchemeClassifierImpl() {}

metrics::OmniboxInputType
AutocompleteSchemeClassifierImpl::GetInputTypeForScheme(
    const std::string& scheme) const {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));
  if (base::IsStringASCII(scheme) &&
      (IsHandledProtocol(scheme) || (scheme == url::kJavaScriptScheme))) {
    return metrics::OmniboxInputType::URL;
  }

  // iOS does not support registration of external schemes.
  return metrics::OmniboxInputType::EMPTY;
}
