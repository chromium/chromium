// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui/search_engine_choice_ui_util.h"

#import "build/branding_buildflags.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_data.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#import "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
#import "third_party/search_engines_data/search_engines_scaled_resources.h"
#import "third_party/search_engines_data/search_engines_scaled_resources_map.h"
#endif  // BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)

class SearchEngineChoiceUiUtilTest : public PlatformTest {};

TEST_F(SearchEngineChoiceUiUtilTest, GetResourceIdFromTemplateURL) {
  TemplateURLData data;
  data.prepopulate_id = TemplateURLPrepopulateData::google.id;
  TemplateURL google_template_url(data);
  const int google_resource_id =
      GetResourceIdFromTemplateURL(google_template_url);
#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
  const int expected_google_resource_id = IDR_SEARCH_ENGINE_GOOGLE_IMAGE;
#else
  const int expected_google_resource_id = IDR_DEFAULT_FAVICON;
#endif  // BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
  EXPECT_EQ(google_resource_id, expected_google_resource_id);

  data.prepopulate_id = TemplateURLPrepopulateData::bing.id;
  TemplateURL bing_template_url(data);
  const int bing_resource_id = GetResourceIdFromTemplateURL(bing_template_url);
#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
  const int expected_bling_resource_id = IDR_SEARCH_ENGINE_BING_IMAGE;
#else
  const int expected_bling_resource_id = IDR_DEFAULT_FAVICON;
#endif  // BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
  EXPECT_EQ(bing_resource_id, expected_bling_resource_id);
}
