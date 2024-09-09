// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_ui_util.h"

#import "base/strings/utf_string_conversions.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#import "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ui/base/resource/resource_bundle.h"

UIImage* SearchEngineFaviconFromTemplateURL(const TemplateURL& template_url) {
  // Only works for prepopulated search engines.
  CHECK_GT(template_url.prepopulate_id(), 0, base::NotFatalUntil::M127)
      << base::UTF16ToUTF8(template_url.short_name());
  std::u16string engine_keyword = template_url.data().keyword();
  int resource_id = search_engines::GetIconResourceId(engine_keyword);
  if (resource_id == -1) {
    // It is possible to have no resource id for a prepopulated search engine
    // that was selected from a country outside of EEA countries.
    return nil;
  }
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  return resource_bundle.GetNativeImageNamed(resource_id).ToUIImage();
}

void GetSearchEngineFavicon(
    const TemplateURL& template_url,
    search_engines::SearchEngineChoiceService* search_engine_choice_service,
    TemplateURLService* template_url_service,
    FaviconLoader* favicon_loader,
    FaviconLoader::FaviconAttributesCompletionBlock favicon_block_handler) {
  if (search_engines::IsEeaChoiceCountry(
          search_engine_choice_service->GetCountryId()) &&
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
