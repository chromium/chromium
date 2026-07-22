// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search_engines/model/search_engines_util.h"

#include "components/search_engines/template_url_service.h"

namespace search_engines {

bool SupportsSearchByImage(TemplateURLService* service) {
  if (!service) {
    return false;
  }
  const TemplateURL* default_url = service->GetDefaultSearchProvider();
  return default_url && !default_url->image_url().empty() &&
         default_url->image_url_ref().IsValid(service->search_terms_data());
}

bool SupportsSearchImageWithLens(TemplateURLService* service) {
  if (!service) {
    return false;
  }
  const TemplateURL* default_url = service->GetDefaultSearchProvider();
  return default_url &&
         default_url->url_ref().HasGoogleBaseURLs(service->search_terms_data());
}

}  // namespace search_engines
