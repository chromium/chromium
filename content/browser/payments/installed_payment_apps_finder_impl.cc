// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/installed_payment_apps_finder_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace content {
namespace {

const char kInstalledPaymentAppsFinderImplName[] =
    "installed_payment_apps_finder_impl";

}  // namespace

// static
base::WeakPtr<InstalledPaymentAppsFinder>
InstalledPaymentAppsFinder::GetInstance(BrowserContext* context) {
  return InstalledPaymentAppsFinderImpl::GetInstance(context);
}

// static
base::WeakPtr<InstalledPaymentAppsFinderImpl>
InstalledPaymentAppsFinderImpl::GetInstance(BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::WeakPtr<InstalledPaymentAppsFinderImpl> result;
  InstalledPaymentAppsFinderImpl* data =
      static_cast<InstalledPaymentAppsFinderImpl*>(
          context->GetUserData(kInstalledPaymentAppsFinderImplName));

  if (!data) {
    auto owned = base::WrapUnique(new InstalledPaymentAppsFinderImpl(context));
    result = owned->weak_ptr_factory_.GetWeakPtr();
    context->SetUserData(kInstalledPaymentAppsFinderImplName, std::move(owned));
  } else {
    result = data->weak_ptr_factory_.GetWeakPtr();
  }

  return result;
}

void InstalledPaymentAppsFinderImpl::GetAllPaymentApps(
    GetAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      browser_context_->GetDefaultStoragePartition());
  scoped_refptr<PaymentAppContextImpl> payment_app_context =
      partition->GetPaymentAppContext();
  payment_app_context->payment_app_database()->ReadAllPaymentApps(
      base::BindOnce(
          &InstalledPaymentAppsFinderImpl::CheckPermissionForPaymentApps,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void InstalledPaymentAppsFinderImpl::CheckPermissionForPaymentApps(
    GetAllPaymentAppsCallback callback,
    PaymentApps apps) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PermissionController* permission_controller =
      browser_context_->GetPermissionController();
  DCHECK(permission_controller);

  PaymentApps permitted_apps;
  for (auto& app : apps) {
    GURL origin = app.second->scope.DeprecatedGetOriginAsURL();
    if (permission_controller
            ->GetPermissionResultForOriginWithoutContext(
                blink::PermissionType::PAYMENT_HANDLER,
                url::Origin::Create(origin))
            .status == blink::mojom::PermissionStatus::GRANTED) {
      permitted_apps[app.first] = std::move(app.second);
    }
  }

  std::move(callback).Run(std::move(permitted_apps));
}

InstalledPaymentAppsFinderImpl::InstalledPaymentAppsFinderImpl(
    BrowserContext* context)
    : browser_context_(context) {}

InstalledPaymentAppsFinderImpl::~InstalledPaymentAppsFinderImpl() = default;

}  // namespace content
