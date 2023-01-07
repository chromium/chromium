// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_DISTRIBUTION_APP_DISTRIBUTION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_DISTRIBUTION_APP_DISTRIBUTION_API_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ios {
namespace provider {

// Returns the app distribution brand code.
std::string GetBrandCode();

// Schedules app distribution notifications to be sent using
// `url_loader_factory`.
void ScheduleAppDistributionNotifications(
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    bool is_first_run);

// Cancels any pending app distribution notifications.
void CancelAppDistributionNotifications();

// Initializes Firebase for installation attribution purpose. `install_date`
// is used to detect "legacy" users that installed Chrome before Firebase was
// integrated and thus should not have Firebase enabled.
void InitializeFirebase(base::Time install_date, bool is_first_run);

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_APP_DISTRIBUTION_APP_DISTRIBUTION_API_H_
