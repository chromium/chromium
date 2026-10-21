// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LANGUAGE_MODEL_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
#define IOS_CHROME_BROWSER_LANGUAGE_MODEL_URL_LANGUAGE_HISTOGRAM_FACTORY_H_

#include <memory>

#include "components/language/core/browser/url_language_histogram.h"
#include "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class UrlLanguageHistogramFactory final
    : public TypedProfileKeyedServiceFactoryIOS<
          UrlLanguageHistogramFactory,
          language::UrlLanguageHistogram> {
 public:
  explicit UrlLanguageHistogramFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
  void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_LANGUAGE_MODEL_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
