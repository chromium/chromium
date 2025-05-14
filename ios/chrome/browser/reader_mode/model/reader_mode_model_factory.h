// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_MODEL_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ReaderModeModel;

// Singleton that owns all ReaderModeModels and associates them with
// profiles.
class ReaderModeModelFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ReaderModeModel* GetForProfile(ProfileIOS* profile);
  static ReaderModeModelFactory* GetInstance();

 private:
  friend class base::NoDestructor<ReaderModeModelFactory>;

  ReaderModeModelFactory();
  ~ReaderModeModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_MODEL_FACTORY_H_
