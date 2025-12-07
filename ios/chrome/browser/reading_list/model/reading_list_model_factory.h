// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ReadingListModel;

namespace reading_list {
class DualReadingListModel;
}  // namespace reading_list

// Singleton that creates the ReadingListModel and associates that service with
// a profile.
class ReadingListModelFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static ReadingListModel* GetForProfile(ProfileIOS* profile);
  static reading_list::DualReadingListModel*
  GetAsDualReadingListModelForProfile(ProfileIOS* profile);
  static ReadingListModelFactory* GetInstance();

 private:
  friend class base::NoDestructor<ReadingListModelFactory>;

  ReadingListModelFactory();
  ~ReadingListModelFactory() override;

  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const override;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_
