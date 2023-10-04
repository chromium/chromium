// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_SERVICE_IOS_H_

#include <string>

#include "components/web_resource/resource_request_allowed_notifier.h"

class GURL;
class PrefService;

namespace language {
class LanguageModel;
}  // namespace language

// Singleton managing the resources required for Translate.
class TranslateServiceIOS
    : public web_resource::ResourceRequestAllowedNotifier::Observer {
 public:
  // Must be called before the Translate feature can be used.
  static void Initialize();

  // Must be called to shut down the Translate feature.
  static void Shutdown();

  // Returns the language to translate to. For more details, see
  // TranslateManager::GetTargetLanguage.
  static std::string GetTargetLanguage(PrefService* prefs,
                                       language::LanguageModel* language_model);

  // Returns true if the URL can be translated.
  static bool IsTranslatableURL(const GURL& url);

  TranslateServiceIOS(const TranslateServiceIOS&) = delete;
  TranslateServiceIOS& operator=(const TranslateServiceIOS&) = delete;

 private:
  TranslateServiceIOS();
  ~TranslateServiceIOS() override;

  // ResourceRequestAllowedNotifier::Observer methods.
  void OnResourceRequestsAllowed() override;

  // Helper class to know if it's allowed to make network resource requests.
  web_resource::ResourceRequestAllowedNotifier
      resource_request_allowed_notifier_;
};

#endif  // IOS_CHROME_BROWSER_TRANSLATE_MODEL_TRANSLATE_SERVICE_IOS_H_
