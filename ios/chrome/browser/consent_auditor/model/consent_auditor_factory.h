// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_
#define IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_

#import "components/consent_auditor/consent_auditor.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all ConsentAuditors and associates them with
// ProfileIOS.
class ConsentAuditorFactory final : public TypedProfileKeyedServiceFactoryIOS<
                                        ConsentAuditorFactory,
                                        consent_auditor::ConsentAuditor> {
 public:
  explicit ConsentAuditorFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_CONSENT_AUDITOR_MODEL_CONSENT_AUDITOR_FACTORY_H_
