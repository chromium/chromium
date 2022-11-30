// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DOWNLOAD_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DOWNLOAD_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class ReadingListDownloadService;

// Singleton that creates the ReadingListDownloadService and associates that
// service with ChromeBrowserState.
class ReadingListDownloadServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static ReadingListDownloadService* GetForBrowserState(
      ChromeBrowserState* browser_state);
  static ReadingListDownloadServiceFactory* GetInstance();

  ReadingListDownloadServiceFactory(const ReadingListDownloadServiceFactory&) =
      delete;
  ReadingListDownloadServiceFactory& operator=(
      const ReadingListDownloadServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ReadingListDownloadServiceFactory>;

  ReadingListDownloadServiceFactory();
  ~ReadingListDownloadServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DOWNLOAD_SERVICE_FACTORY_H_
