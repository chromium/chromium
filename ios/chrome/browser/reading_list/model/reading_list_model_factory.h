// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ReadingListModel;

namespace reading_list {
class DualReadingListModel;
}  // namespace reading_list

// Singleton that creates the ReadingListModel and associates that service with
// a profile.
class ReadingListModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static ReadingListModel* GetForBrowserState(ProfileIOS* profile);

  static ReadingListModel* GetForProfile(ProfileIOS* profile);
  static reading_list::DualReadingListModel*
  GetAsDualReadingListModelForProfile(ProfileIOS* profile);
  static ReadingListModelFactory* GetInstance();

  ReadingListModelFactory(const ReadingListModelFactory&) = delete;
  ReadingListModelFactory& operator=(const ReadingListModelFactory&) = delete;

 private:
  friend class base::NoDestructor<ReadingListModelFactory>;

  ReadingListModelFactory();
  ~ReadingListModelFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_MODEL_FACTORY_H_
