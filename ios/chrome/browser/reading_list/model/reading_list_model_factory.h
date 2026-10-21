// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_

#import <memory>

#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/shared/model/profile/typed_profile_keyed_service_factory_ios.h"

namespace reading_list {
class DualReadingListModel;
}  // namespace reading_list

// Singleton that creates the ReadingListModel and associates that service with
// a profile.
class ReadingListModelFactory final
    : public TypedProfileKeyedServiceFactoryIOS<ReadingListModelFactory,
                                                ReadingListModel> {
 public:
  explicit ReadingListModelFactory(PassKey key);
  static reading_list::DualReadingListModel*
  GetAsDualReadingListModelForProfile(ProfileIOS* profile);

 private:
  // ProfileKeyedServiceFactoryIOS implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      ProfileIOS* profile) const final;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_
