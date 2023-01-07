// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class ReadingListModel;

// Singleton that creates the ReadingListModel and associates that service with
// ChromeBrowserState.
class ReadingListModelFactory : public BrowserStateKeyedServiceFactory {
 public:
  static ReadingListModel* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static ReadingListModelFactory* GetInstance();

  ReadingListModelFactory(const ReadingListModelFactory&) = delete;
  ReadingListModelFactory& operator=(const ReadingListModelFactory&) = delete;

  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

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

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
