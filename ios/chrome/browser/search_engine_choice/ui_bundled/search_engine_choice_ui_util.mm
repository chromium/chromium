// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engine_choice/ui_bundled/search_engine_choice_ui_util.h"

#import <algorithm>
#import <string>

#import "base/strings/utf_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/grit/components_scaled_resources.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/resources/grit/ui_resources.h"

#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
#import "third_party/search_engines_data/search_engines_scaled_resources_map.h"
#endif

namespace {
int GetResourceIdFromTemplateURL(const TemplateURL& template_url) {
#if BUILDFLAG(ENABLE_BUILTIN_SEARCH_PROVIDER_ASSETS)
  // This would be better served by ResourcesUtil::GetThemeResourceId(), but
  // the symbol appears to be unreachable from the ios/chrome/browser.
  std::string resource_name = template_url.GetBuiltinImageResourceId();
  auto resource_it = std::ranges::find_if(
      kSearchEnginesScaledResources,
      [&](const auto& resource) { return resource.path == resource_name; });

  // Note: it is possible to have no resource id for a prepopulated search
  // engine that was selected from a country outside of EEA countries.
  if (resource_it != std::end(kSearchEnginesScaledResources)) {
    return resource_it->id;
  }

  if (resource_name == "IDR_SEARCH_ENGINE_GOOGLE_IMAGE") {
    // Unlike the other logos which are in `kSearchEnginesScaledResources`,
    // the Google logo is included via
    // `components/resources/search_engine_choice_scaled_resources.grdp`
    // TODO(crbug.com/422992330): Fix this discrepancy now that all OSE assets
    // are restricted to branded builds.
    return IDR_SEARCH_ENGINE_GOOGLE_IMAGE;
  }
#endif

  return IDR_DEFAULT_FAVICON;
}
}  // namespace

UIImage* SearchEngineFaviconFromTemplateURL(const TemplateURL& template_url) {
  // Only works for prepopulated search engines.
  CHECK_GT(template_url.prepopulate_id(), 0)
      << base::UTF16ToUTF8(template_url.short_name());

  int resource_id = GetResourceIdFromTemplateURL(template_url);
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  return resource_bundle.GetNativeImageNamed(resource_id).ToUIImage();
}

void GetSearchEngineFavicon(
    const TemplateURL& template_url,
    regional_capabilities::RegionalCapabilitiesService& regional_capabilities,
    TemplateURLService* template_url_service,
    FaviconLoader* favicon_loader,
    FaviconLoader::FaviconAttributesCompletionBlock favicon_block_handler) {
  if (regional_capabilities.IsInEeaCountry() &&
      template_url.prepopulate_id() > 0) {
    // For EEA countries, embedded favicons should be prefered, with a fall
    // back using FaviconLoader APIs.
    UIImage* image = SearchEngineFaviconFromTemplateURL(template_url);
    if (image) {
      FaviconAttributes* attributes =
          [FaviconAttributes attributesWithImage:image];
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0 * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            favicon_block_handler(attributes);
          });
      return;
    }
  }
  if (template_url.prepopulate_id() > 0) {
    GURL itemURL = GURL(template_url.url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(std::u16string()),
        template_url_service->search_terms_data()));
    favicon_loader->FaviconForPageUrl(
        itemURL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/YES, favicon_block_handler);
  } else {
    GURL itemURL = template_url.favicon_url();
    favicon_loader->FaviconForIconUrl(itemURL, kDesiredMediumFaviconSizePt,
                                      kMinFaviconSizePt, favicon_block_handler);
  }
}
