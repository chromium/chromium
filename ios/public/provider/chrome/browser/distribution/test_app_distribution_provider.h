// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISTRIBUTION_TEST_APP_DISTRIBUTION_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISTRIBUTION_TEST_APP_DISTRIBUTION_PROVIDER_H_

#import "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class TestAppDistributionProvider : public AppDistributionProvider {
 public:
  TestAppDistributionProvider();
  ~TestAppDistributionProvider() override;

  // AppDistributionProvider.
  std::string GetDistributionBrandCode() override;
  void ScheduleDistributionNotifications(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      bool is_first_run) override;
  void CancelDistributionNotifications() override;
  bool IsPreFirebaseLegacyUser(int64_t install_date) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppDistributionProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISTRIBUTION_TEST_APP_DISTRIBUTION_PROVIDER_H_
