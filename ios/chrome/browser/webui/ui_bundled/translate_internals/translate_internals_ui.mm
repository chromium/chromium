// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/translate_internals/translate_internals_ui.h"

#import <string>

#import "components/translate/core/common/translate_util.h"
#import "components/translate/translate_internals/translate_internals_handler.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/translate_internals/ios_translate_internals_handler.h"
#import "ios/chrome/grit/ios_resources.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"

namespace {

// Creates a WebUI data source for chrome://translate-internals page.
// Changes to this should be in sync with its non-iOS equivalent
// chrome/browser/ui/webui/translate_internals/translate_internals_ui.cc
web::WebUIIOSDataSource* CreateTranslateInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUITranslateInternalsHost);

  source->SetDefaultResource(IDR_IOS_TRANSLATE_INTERNALS_HTML);
  source->UseStringsJs();
  source->AddResourcePath("translate_internals.css",
                          IDR_IOS_TRANSLATE_INTERNALS_CSS);
  source->AddResourcePath("translate_internals.js",
                          IDR_IOS_TRANSLATE_INTERNALS_JS);

  base::Value::Dict langs =
      translate::TranslateInternalsHandler::GetLanguages();
  for (const auto key_value_pair : langs) {
    DCHECK(key_value_pair.second.is_string());
    std::string key = "language-" + key_value_pair.first;
    const std::string& value = key_value_pair.second.GetString();
    source->AddString(key, value);
  }

  if (translate::IsTFLiteLanguageDetectionEnabled()) {
    source->AddString("model-version", "TFLite_v1");
  } else {
    // The default language detection model is "CLD3".
    source->AddString("model-version", "CLD3");
  }

  return source;
}

}  // namespace

TranslateInternalsUI::TranslateInternalsUI(web::WebUIIOS* web_ui,
                                           const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<IOSTranslateInternalsHandler>());
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateTranslateInternalsHTMLSource());
}

TranslateInternalsUI::~TranslateInternalsUI() {}
