// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_FACTORY_H_
#define CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/loader/navigation_url_loader_factory.h"

namespace content {

// Manages creation of the NavigationURLLoaders; when registered, all created
// NavigationURLLoaderss will be TestNavigationURLLoaderss. This automatically
// registers itself when it goes in scope, and unregisters itself when it goes
// out of scope. Since you can't have more than one factory registered at a
// time, you can only have one of these objects at a time.
class TestNavigationURLLoaderFactory : public NavigationURLLoaderFactory {
 public:
  TestNavigationURLLoaderFactory();
  ~TestNavigationURLLoaderFactory() override;

  // TestNavigationURLLoaderFactory implementation.
  std::unique_ptr<NavigationURLLoader> CreateLoader(
      StoragePartition* storage_partition,
      std::unique_ptr<NavigationRequestInfo> request_info,
      std::unique_ptr<NavigationUIData> navigation_ui_data,
      ServiceWorkerNavigationHandle* service_worker_handle,
      NavigationURLLoaderDelegate* delegate,
      bool is_served_from_back_forward_cache) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNavigationURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_NAVIGATION_URL_LOADER_FACTORY_H_
