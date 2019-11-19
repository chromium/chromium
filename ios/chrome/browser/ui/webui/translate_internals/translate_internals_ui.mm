// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/translate_internals/translate_internals_ui.h"

#include <string>

#import "components/translate/translate_internals/translate_internals_handler.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/webui/translate_internals/ios_translate_internals_handler.h"
#include "ios/chrome/grit/ios_resources.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Creates a WebUI data source for chrome://translate-internals page.
// Changes to this should be in sync with its non-iOS equivalent
// chrome/browser/ui/webui/translate_internals/translate_internals_ui.cc
web::WebUIIOSDataSource* CreateTranslateInternalsHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUITranslateInternalsHost);

  source->SetDefaultResource(IDR_IOS_TRANSLATE_INTERNALS_HTML);
  source->UseStringsJs();
  source->AddResourcePath("translate_internals.js",
                          IDR_IOS_TRANSLATE_INTERNALS_JS);

  base::DictionaryValue langs;
  translate::TranslateInternalsHandler::GetLanguages(&langs);
  for (base::DictionaryValue::Iterator it(langs); !it.IsAtEnd(); it.Advance()) {
    std::string key = "language-" + it.key();
    std::string value;
    it.value().GetAsString(&value);
    source->AddString(key, value);
  }

  source->AddString("cld-version", "3");

  return source;
}

}  // namespace

TranslateInternalsUI::TranslateInternalsUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<IOSTranslateInternalsHandler>());
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateTranslateInternalsHTMLSource());
}

TranslateInternalsUI::~TranslateInternalsUI() {}
