// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_FACTORY_H_
#define IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_FACTORY_H_

#import "ios/chrome/browser/external_files/model/external_file_remover.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

// Singleton that owns all `ExternalFileRemover` and associates them with
// profiles. Listens for the `ProfileIOS`'s destruction notification and
// cleans up the associated `ExternalFileRemover`.
class ExternalFileRemoverFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ExternalFileRemoverFactory,
                                                ExternalFileRemover> {
 public:
  explicit ExternalFileRemoverFactory(PassKey key);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_EXTERNAL_FILES_MODEL_EXTERNAL_FILE_REMOVER_FACTORY_H_
