// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISTRIBUTION_APP_DISTRIBUTION_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISTRIBUTION_APP_DISTRIBUTION_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class AppDistributionProvider {
 public:
  AppDistributionProvider();
  virtual ~AppDistributionProvider();

  // Returns the distribution brand code.
  virtual std::string GetDistributionBrandCode();

  // Schedules distribution notifications to be sent using the given |context|.
  virtual void ScheduleDistributionNotifications(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      bool is_first_run);

  // Cancels any pending distribution notifications.
  virtual void CancelDistributionNotifications();

  // Returns whether user who installed Chrome on |install_date| predates
  // integration with Firebase for installation attribution.
  virtual bool IsPreFirebaseLegacyUser(int64_t install_date);

 private:
  DISALLOW_COPY_AND_ASSIGN(AppDistributionProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_DISTRIBUTION_APP_DISTRIBUTION_PROVIDER_H_
