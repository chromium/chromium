// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_database.h"

#include <map>
#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/payments/payment_app.pb.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

using ::payments::mojom::PaymentDelegation;
using ::payments::mojom::PaymentHandlerStatus;

const char kPaymentAppPrefix[] = "PaymentApp:";
const char kPaymentInstrumentPrefix[] = "PaymentInstrument:";

// |pattern| is the scope URL of the service worker registration.
std::string CreatePaymentAppKey(const std::string& pattern) {
  return kPaymentAppPrefix + pattern;
}

std::string CreatePaymentInstrumentKey(const std::string& instrument_key) {
  return kPaymentInstrumentPrefix + instrument_key;
}

SupportedDelegations ToSupportedDelegations(
    const content::SupportedDelegationsProto& supported_delegations_proto) {
  SupportedDelegations supported_delegations;
  if (supported_delegations_proto.has_shipping_address()) {
    supported_delegations.shipping_address =
        supported_delegations_proto.shipping_address();
  }
  if (supported_delegations_proto.has_payer_name()) {
    supported_delegations.payer_name = supported_delegations_proto.payer_name();
  }
  if (supported_delegations_proto.has_payer_email()) {
    supported_delegations.payer_email =
        supported_delegations_proto.payer_email();
  }
  if (supported_delegations_proto.has_payer_phone()) {
    supported_delegations.payer_phone =
        supported_delegations_proto.payer_phone();
  }

  return supported_delegations;
}

std::unique_ptr<StoredPaymentApp> ToStoredPaymentApp(const std::string& input) {
  StoredPaymentAppProto app_proto;
  if (!app_proto.ParseFromString(input))
    return nullptr;

  std::unique_ptr<StoredPaymentApp> app = std::make_unique<StoredPaymentApp>();
  app->registration_id = app_proto.registration_id();
  app->scope = GURL(app_proto.scope());
  app->name = app_proto.name();
  app->prefer_related_applications = app_proto.prefer_related_applications();
  for (const auto& related_app : app_proto.related_applications()) {
    app->related_applications.emplace_back(StoredRelatedApplication());
    app->related_applications.back().platform = related_app.platform();
    app->related_applications.back().id = related_app.id();
  }
  app->user_hint = app_proto.user_hint();
  app->supported_delegations =
      ToSupportedDelegations(app_proto.supported_delegations());

  if (!app_proto.icon().empty()) {
    std::optional<std::vector<uint8_t>> icon_raw_data =
        base::Base64Decode(app_proto.icon());
    app->icon = std::make_unique<SkBitmap>();
    // Note that the icon has been decoded to PNG raw data regardless of the
    // original icon format that was downloaded.
    *app->icon = gfx::PNGCodec::Decode(icon_raw_data.value());
    CHECK(!app->icon->isNull());
  }

  return app;
}

}  // namespace

PaymentAppDatabase::PaymentAppDatabase(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PaymentAppDatabase::~PaymentAppDatabase() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppDatabase::ReadAllPaymentApps(
    ReadAllPaymentAppsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kPaymentAppPrefix,
      base::BindOnce(&PaymentAppDatabase::DidReadAllPaymentApps,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::SetPaymentAppUserHint(const GURL& scope,
                                               const std::string& user_hint) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToSetPaymentAppUserHint,
          weak_ptr_factory_.GetWeakPtr(), user_hint));
}

void PaymentAppDatabase::EnablePaymentAppDelegations(
    const GURL& scope,
    const std::vector<PaymentDelegation>& delegations,
    EnableDelegationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToEnablePaymentAppDelegations,
          weak_ptr_factory_.GetWeakPtr(), delegations, std::move(callback)));
}

void PaymentAppDatabase::DidFindRegistrationToEnablePaymentAppDelegations(
    const std::vector<PaymentDelegation>& delegations,
    EnableDelegationsCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  // Constructing registration_id and payment_app_key before
  // moving registration.
  int64_t registration_id = registration->id();
  std::string payment_app_key =
      CreatePaymentAppKey(registration->scope().spec());
  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      registration_id, payment_app_key,
      base::BindOnce(
          &PaymentAppDatabase::DidGetPaymentAppInfoToEnableDelegations,
          weak_ptr_factory_.GetWeakPtr(), delegations, std::move(callback),
          std::move(registration)));
}

void PaymentAppDatabase::DidGetPaymentAppInfoToEnableDelegations(
    const std::vector<PaymentDelegation>& delegations,
    EnableDelegationsCallback callback,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  DCHECK_LE(data.size(), 1U);
  StoredPaymentAppProto app_proto;
  if (data.size() == 1U) {
    app_proto.ParseFromString(data[0]);
  }

  auto supported_delegations_proto =
      std::make_unique<SupportedDelegationsProto>();
  for (auto delegation : delegations) {
    switch (delegation) {
      case PaymentDelegation::SHIPPING_ADDRESS:
        supported_delegations_proto->set_shipping_address(true);
        break;
      case PaymentDelegation::PAYER_NAME:
        supported_delegations_proto->set_payer_name(true);
        break;
      case PaymentDelegation::PAYER_PHONE:
        supported_delegations_proto->set_payer_phone(true);
        break;
      case PaymentDelegation::PAYER_EMAIL:
        supported_delegations_proto->set_payer_email(true);
        break;
    }
  }
  app_proto.set_allocated_supported_delegations(
      supported_delegations_proto.release());

  std::string serialized_payment_app;
  bool success = app_proto.SerializeToString(&serialized_payment_app);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->key(),
      {{CreatePaymentAppKey(registration->scope().spec()),
        serialized_payment_app}},
      base::BindOnce(&PaymentAppDatabase::DidEnablePaymentAppDelegations,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidEnablePaymentAppDelegations(
    EnableDelegationsCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(callback).Run(
      status == blink::ServiceWorkerStatusCode::kOk
          ? PaymentHandlerStatus::SUCCESS
          : PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
}

void PaymentAppDatabase::DidFindRegistrationToSetPaymentAppUserHint(
    const std::string& user_hint,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk)
    return;

  // Constructing registration_id and payment_app_key before
  // moving registration.
  int64_t registration_id = registration->id();
  std::string payment_app_key =
      CreatePaymentAppKey(registration->scope().spec());
  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      registration_id, payment_app_key,
      base::BindOnce(&PaymentAppDatabase::DidGetPaymentAppInfoToSetUserHint,
                     weak_ptr_factory_.GetWeakPtr(), user_hint,
                     std::move(registration)));
}

void PaymentAppDatabase::DidGetPaymentAppInfoToSetUserHint(
    const std::string& user_hint,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk)
    return;

  DCHECK_LE(data.size(), 1U);
  StoredPaymentAppProto app_proto;
  if (data.size() == 1U) {
    app_proto.ParseFromString(data[0]);
  }
  app_proto.set_user_hint(user_hint);

  std::string serialized_payment_app;
  bool success = app_proto.SerializeToString(&serialized_payment_app);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->key(),
      {{CreatePaymentAppKey(registration->scope().spec()),
        serialized_payment_app}},
      base::BindOnce(&PaymentAppDatabase::DidSetPaymentAppUserHint,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PaymentAppDatabase::DidSetPaymentAppUserHint(
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(status == blink::ServiceWorkerStatusCode::kOk);
}

void PaymentAppDatabase::SetPaymentAppInfoForRegisteredServiceWorker(
    int64_t registration_id,
    const std::string& instrument_key,
    const std::string& name,
    const std::string& icon,
    const std::string& method,
    const SupportedDelegations& supported_delegations,
    SetPaymentAppInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context_->FindReadyRegistrationForIdOnly(
      registration_id,
      base::BindOnce(&PaymentAppDatabase::DidFindRegistrationToSetPaymentApp,
                     weak_ptr_factory_.GetWeakPtr(), instrument_key, name, icon,
                     method, supported_delegations, std::move(callback)));
}

void PaymentAppDatabase::DidFindRegistrationToSetPaymentApp(
    const std::string& instrument_key,
    const std::string& name,
    const std::string& icon,
    const std::string& method,
    const SupportedDelegations& supported_delegations,
    SetPaymentAppInfoCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  StoredPaymentAppProto payment_app_proto;
  payment_app_proto.set_registration_id(registration->id());
  payment_app_proto.set_scope(registration->scope().spec());
  payment_app_proto.set_name(name);
  payment_app_proto.set_icon(icon);

  // Set supported delegations.
  auto supported_delegations_proto =
      std::make_unique<SupportedDelegationsProto>();
  supported_delegations_proto->set_shipping_address(
      supported_delegations.shipping_address);
  supported_delegations_proto->set_payer_name(supported_delegations.payer_name);
  supported_delegations_proto->set_payer_phone(
      supported_delegations.payer_phone);
  supported_delegations_proto->set_payer_email(
      supported_delegations.payer_email);
  payment_app_proto.set_allocated_supported_delegations(
      supported_delegations_proto.release());

  std::string serialized_payment_app;
  bool success = payment_app_proto.SerializeToString(&serialized_payment_app);
  DCHECK(success);

  // Constructing registration_id, registration_key and storage_key before
  // moving registration.
  int64_t registration_id = registration->id();
  blink::StorageKey registration_key = registration->key();
  std::string storage_key = CreatePaymentAppKey(registration->scope().spec());
  service_worker_context_->StoreRegistrationUserData(
      registration_id, registration_key,
      {{storage_key, serialized_payment_app}},
      base::BindOnce(&PaymentAppDatabase::DidWritePaymentAppForSetPaymentApp,
                     weak_ptr_factory_.GetWeakPtr(), instrument_key, method,
                     std::move(callback), std::move(registration)));

  return;
}

void PaymentAppDatabase::DidWritePaymentAppForSetPaymentApp(
    const std::string& instrument_key,
    const std::string& method,
    SetPaymentAppInfoCallback callback,
    scoped_refptr<ServiceWorkerRegistration> registration,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
    return;
  }

  StoredPaymentInstrumentProto instrument_proto;
  instrument_proto.set_registration_id(registration->id());
  instrument_proto.set_instrument_key(instrument_key);
  instrument_proto.set_method(method);

  std::string serialized_instrument;
  bool success = instrument_proto.SerializeToString(&serialized_instrument);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->key(),
      {{CreatePaymentInstrumentKey(instrument_key), serialized_instrument}},
      base::BindOnce(
          &PaymentAppDatabase::DidWritePaymentInstrumentForSetPaymentApp,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidWritePaymentInstrumentForSetPaymentApp(
    SetPaymentAppInfoCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(callback).Run(
      status == blink::ServiceWorkerStatusCode::kOk
          ? PaymentHandlerStatus::SUCCESS
          : PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
}

void PaymentAppDatabase::DidReadAllPaymentApps(
    ReadAllPaymentAppsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& raw_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentApps());
    return;
  }

  PaymentApps apps;
  for (const auto& item_of_raw_data : raw_data) {
    std::unique_ptr<StoredPaymentApp> app =
        ToStoredPaymentApp(item_of_raw_data.second);
    if (app)
      apps[app->registration_id] = std::move(app);
  }

  if (apps.size() == 0U) {
    std::move(callback).Run(PaymentApps());
    return;
  }

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      kPaymentInstrumentPrefix,
      base::BindOnce(&PaymentAppDatabase::DidReadAllPaymentInstruments,
                     weak_ptr_factory_.GetWeakPtr(), std::move(apps),
                     std::move(callback)));
}

void PaymentAppDatabase::DidReadAllPaymentInstruments(
    PaymentApps apps,
    ReadAllPaymentAppsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& raw_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentApps());
    return;
  }

  for (const auto& item_of_raw_data : raw_data) {
    StoredPaymentInstrumentProto instrument_proto;
    if (!instrument_proto.ParseFromString(item_of_raw_data.second))
      continue;

    int64_t id = instrument_proto.registration_id();
    if (!apps.contains(id))
      continue;

    apps[id]->enabled_methods.emplace_back(instrument_proto.method());
  }

  std::move(callback).Run(std::move(apps));
}


}  // namespace content
