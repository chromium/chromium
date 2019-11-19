// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_FACTORY_H_
#define IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace ios {
class ChromeBrowserState;
}

class ExternalFileRemover;

// Singleton that owns all |ExternalFileRemover| and associates them with
// browser states. Listens for the |BrowserState|'s destruction notification and
// cleans up the associated |ExternalFileRemover|.
class ExternalFileRemoverFactory : public BrowserStateKeyedServiceFactory {
 public:
  static ExternalFileRemover* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);
  static ExternalFileRemoverFactory* GetInstance();

 private:
  friend class base::NoDestructor<ExternalFileRemoverFactory>;

  ExternalFileRemoverFactory();
  ~ExternalFileRemoverFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ExternalFileRemoverFactory);
};

#endif  // IOS_CHROME_BROWSER_EXTERNAL_FILES_EXTERNAL_FILE_REMOVER_FACTORY_H_
