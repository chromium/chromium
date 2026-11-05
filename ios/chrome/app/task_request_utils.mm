// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_utils.h"

#import <string>

#import "base/strings/sys_string_conversions.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "url/gurl.h"

GURL GenerateResultGURLFromSearchQuery(NSString* search_query,
                                       ProfileIOS* profile) {
  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  const TemplateURL* default_url =
      template_url_service->GetDefaultSearchProvider();
  if (default_url && !default_url->url().empty() &&
      default_url->url_ref().IsValid(
          template_url_service->search_terms_data())) {
    std::u16string query_string = base::SysNSStringToUTF16(search_query);
    TemplateURLRef::SearchTermsArgs search_args(query_string);
    return GURL(default_url->url_ref().ReplaceSearchTerms(
        search_args, template_url_service->search_terms_data()));
  }
  return GURL();
}
