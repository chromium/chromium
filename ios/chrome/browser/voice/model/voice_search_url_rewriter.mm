// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/voice_search_url_rewriter.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/voice/model/speech_input_locale_config.h"
#import "ios/chrome/browser/voice/model/voice_search_prefs.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

bool VoiceSearchURLRewriter(GURL* url, web::BrowserState* browser_state) {
  if (!google_util::IsGoogleSearchUrl(*url)) {
    return false;
  }

  ProfileIOS* profile = ProfileIOS::FromBrowserState(browser_state);
  std::string language =
      profile->GetPrefs()->GetString(prefs::kVoiceSearchLocale);
  GURL rewritten_url(*url);
  // The `hl` parameter will be overriden only if the voice search locale
  // is not empty. If it is empty (indicating that voice search locale
  // uses device language), the `hl` will keep the original value.
  // If there is no `hl` in the query the `spknlang` will use the application
  // locale as a fallback (instead of using the same locale for both `hl`
  // and `spknlang`).
  if (language.empty()) {
    voice::SpeechInputLocaleConfig* locale_config =
        voice::SpeechInputLocaleConfig::GetInstance();
    if (locale_config) {
      language = locale_config->GetDefaultLocale().code;
    }
    if (!language.length()) {
      NOTREACHED_IN_MIGRATION();
      language = "en-US";
    }
  }
  rewritten_url =
      net::AppendOrReplaceQueryParameter(rewritten_url, "hl", language);
  rewritten_url =
      net::AppendQueryParameter(rewritten_url, "spknlang", language);
  rewritten_url = net::AppendQueryParameter(rewritten_url, "inm", "vs");
  rewritten_url = net::AppendQueryParameter(rewritten_url, "vse", "1");
  *url = rewritten_url;

  // Return false so other URLRewriters can update the url if necessary.
  return false;
}
