// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_INSTALLED_PAYMENT_APPS_FINDER_IMPL_H_
#define CONTENT_BROWSER_PAYMENTS_INSTALLED_PAYMENT_APPS_FINDER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/installed_payment_apps_finder.h"

namespace content {

class CONTENT_EXPORT InstalledPaymentAppsFinderImpl
    : public InstalledPaymentAppsFinder,
      public base::SupportsUserData::Data {
 public:
  static base::WeakPtr<InstalledPaymentAppsFinderImpl> GetInstance(
      BrowserContext* context);
  ~InstalledPaymentAppsFinderImpl() override;

  // Disallow copy and assign.
  InstalledPaymentAppsFinderImpl(const InstalledPaymentAppsFinderImpl& other) =
      delete;
  InstalledPaymentAppsFinderImpl& operator=(
      const InstalledPaymentAppsFinderImpl& other) = delete;

  void GetAllPaymentApps(GetAllPaymentAppsCallback callback) override;

 private:
  explicit InstalledPaymentAppsFinderImpl(BrowserContext* context);
  void CheckPermissionForPaymentApps(GetAllPaymentAppsCallback callback,
                                     PaymentApps apps);

  raw_ptr<BrowserContext> browser_context_;
  base::WeakPtrFactory<InstalledPaymentAppsFinderImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_INSTALLED_PAYMENT_APPS_FINDER_IMPL_H_
