// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_FACTORY_H_
#define IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ExternalFileRemover;

// Singleton that owns all `ExternalFileRemover` and associates them with
// profiles. Listens for the `ProfileIOS`'s destruction notification and
// cleans up the associated `ExternalFileRemover`.
class ExternalFileRemoverFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static ExternalFileRemover* GetForBrowserState(ProfileIOS* profile);

  static ExternalFileRemover* GetForProfile(ProfileIOS* profile);
  static ExternalFileRemoverFactory* GetInstance();

  ExternalFileRemoverFactory(const ExternalFileRemoverFactory&) = delete;
  ExternalFileRemoverFactory& operator=(const ExternalFileRemoverFactory&) =
      delete;

 private:
  friend class base::NoDestructor<ExternalFileRemoverFactory>;

  ExternalFileRemoverFactory();
  ~ExternalFileRemoverFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_FACTORY_H_
