// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INSTALLED_PAYMENT_APPS_FINDER_H_
#define CONTENT_PUBLIC_BROWSER_INSTALLED_PAYMENT_APPS_FINDER_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/stored_payment_app.h"

namespace content {

class BrowserContext;

// This is helper class for retrieving installed payment apps.
// The instance of this class can be retrieved using the static
// GetInstance() method. All methods must be called on the UI thread.

// Example:
//    base::WeakPtr<InstalledPaymentAppsFinder> finder =
//      content::InstalledPaymentAppsFinder::GetInstance(context);

class CONTENT_EXPORT InstalledPaymentAppsFinder {
 public:
  // All functions are actually implemented in
  // InstalledPaymentAppsFinderImpl.cc. Please see:
  // content/browser/payments/installed_payment_apps_finder.cc

  // This class is owned BrowserContext, and will be reuse the existing one
  // using GetInstance() if an instance already exists with BrowserContext.
  static base::WeakPtr<InstalledPaymentAppsFinder> GetInstance(
      BrowserContext* context);

  using PaymentApps = std::map<int64_t, std::unique_ptr<StoredPaymentApp>>;
  using GetAllPaymentAppsCallback = base::OnceCallback<void(PaymentApps)>;

  // This method is used to query all registered payment apps with payment
  // instruments in browser side. When merchant site requests payment to browser
  // via PaymentRequest API, then the UA will display the all proper stored
  // payment instruments by this method.
  virtual void GetAllPaymentApps(GetAllPaymentAppsCallback callback) = 0;

 protected:
  virtual ~InstalledPaymentAppsFinder() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INSTALLED_PAYMENT_APPS_FINDER_H_
