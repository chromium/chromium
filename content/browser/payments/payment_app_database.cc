// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_database.h"

#include <map>
#include <utility>

#include "base/base64.h"
#include "base/containers/contains.h"
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
using ::payments::mojom::PaymentInstrument;
using ::payments::mojom::PaymentInstrumentPtr;

const char kPaymentAppPrefix[] = "PaymentApp:";
const char kPaymentInstrumentPrefix[] = "PaymentInstrument:";
const char kPaymentInstrumentKeyInfoPrefix[] = "PaymentInstrumentKeyInfo:";

// |pattern| is the scope URL of the service worker registration.
std::string CreatePaymentAppKey(const std::string& pattern) {
  return kPaymentAppPrefix + pattern;
}

std::string CreatePaymentInstrumentKey(const std::string& instrument_key) {
  return kPaymentInstrumentPrefix + instrument_key;
}

std::string CreatePaymentInstrumentKeyInfoKey(
    const std::string& instrument_key) {
  return kPaymentInstrumentKeyInfoPrefix + instrument_key;
}

std::map<uint64_t, std::string> ToStoredPaymentInstrumentKeyInfos(
    const std::vector<std::string>& inputs) {
  std::map<uint64_t, std::string> key_info;
  for (const auto& input : inputs) {
    StoredPaymentInstrumentKeyInfoProto key_info_proto;
    if (!key_info_proto.ParseFromString(input))
      return std::map<uint64_t, std::string>();

    key_info.insert(std::pair<uint64_t, std::string>(
        key_info_proto.insertion_order(), key_info_proto.key()));
  }

  return key_info;
}

PaymentInstrumentPtr ToPaymentInstrumentForMojo(const std::string& input) {
  StoredPaymentInstrumentProto instrument_proto;
  if (!instrument_proto.ParseFromString(input))
    return nullptr;

  PaymentInstrumentPtr instrument = PaymentInstrument::New();
  instrument->name = instrument_proto.name();
  for (const auto& icon_proto : instrument_proto.icons()) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL(icon_proto.src());
    icon.type = base::UTF8ToUTF16(icon_proto.type());
    for (const auto& size_proto : icon_proto.sizes()) {
      icon.sizes.emplace_back(size_proto.width(), size_proto.height());
    }
    instrument->icons.emplace_back(icon);
  }
  instrument->method = instrument_proto.method();

  return instrument;
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
    std::string icon_raw_data;
    base::Base64Decode(app_proto.icon(), &icon_raw_data);
    app->icon = std::make_unique<SkBitmap>();
    // Note that the icon has been decoded to PNG raw data regardless of the
    // original icon format that was downloaded.
    bool success = gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(icon_raw_data.data()),
        icon_raw_data.size(), app->icon.get());
    DCHECK(success);
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

void PaymentAppDatabase::DeletePaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToDeletePaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key, std::move(callback)));
}

void PaymentAppDatabase::ReadPaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    ReadPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToReadPaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key, std::move(callback)));
}

void PaymentAppDatabase::KeysOfPaymentInstruments(
    const GURL& scope,
    KeysOfPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(&PaymentAppDatabase::DidFindRegistrationToGetKeys,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::HasPaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    HasPaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToHasPaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key, std::move(callback)));
}

void PaymentAppDatabase::WritePaymentInstrument(
    const GURL& scope,
    const std::string& instrument_key,
    PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  if (instrument->icons.size() > 0) {
    std::vector<blink::Manifest::ImageResource> icons(instrument->icons);
    PaymentInstrumentIconFetcher::Start(
        scope,
        service_worker_context_->GetWindowClientFrameRoutingIds(
            blink::StorageKey::CreateFirstParty(url::Origin::Create(scope))),
        icons,
        base::BindOnce(&PaymentAppDatabase::DidFetchedPaymentInstrumentIcon,
                       weak_ptr_factory_.GetWeakPtr(), scope, instrument_key,
                       std::move(instrument), std::move(callback)));
  } else {
    service_worker_context_->FindReadyRegistrationForScope(
        scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
        base::BindOnce(
            &PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument,
            weak_ptr_factory_.GetWeakPtr(), instrument_key,
            std::move(instrument), std::string(), std::move(callback)));
  }
}

void PaymentAppDatabase::DidFetchedPaymentInstrumentIcon(
    const GURL& scope,
    const std::string& instrument_key,
    payments::mojom::PaymentInstrumentPtr instrument,
    WritePaymentInstrumentCallback callback,
    const std::string& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (icon.empty()) {
    std::move(callback).Run(PaymentHandlerStatus::FETCH_INSTRUMENT_ICON_FAILED);
    return;
  }

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument,
          weak_ptr_factory_.GetWeakPtr(), instrument_key, std::move(instrument),
          icon, std::move(callback)));
}

void PaymentAppDatabase::FetchAndUpdatePaymentAppInfo(
    const GURL& context,
    const GURL& scope,
    FetchAndUpdatePaymentAppInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PaymentAppInfoFetcher::Start(
      context, service_worker_context_,
      base::BindOnce(&PaymentAppDatabase::FetchPaymentAppInfoCallback,
                     weak_ptr_factory_.GetWeakPtr(), scope,
                     std::move(callback)));
}

void PaymentAppDatabase::FetchPaymentAppInfoCallback(
    const GURL& scope,
    FetchAndUpdatePaymentAppInfoCallback callback,
    std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToUpdatePaymentAppInfo,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          std::move(app_info)));
}

void PaymentAppDatabase::DidFindRegistrationToUpdatePaymentAppInfo(
    FetchAndUpdatePaymentAppInfoCallback callback,
    std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      registration->id(), CreatePaymentAppKey(registration->scope().spec()),
      base::BindOnce(
          &PaymentAppDatabase::DidGetPaymentAppInfoToUpdatePaymentAppInfo,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback),
          std::move(app_info), registration));
}

void PaymentAppDatabase::DidGetPaymentAppInfoToUpdatePaymentAppInfo(
    FetchAndUpdatePaymentAppInfoCallback callback,
    std::unique_ptr<PaymentAppInfoFetcher::PaymentAppInfo> app_info,
    scoped_refptr<ServiceWorkerRegistration> registration,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  DCHECK_LE(data.size(), 1U);
  StoredPaymentAppProto payment_app_proto;
  if (data.size() == 1U) {
    payment_app_proto.ParseFromString(data[0]);
  }
  payment_app_proto.set_registration_id(registration->id());
  payment_app_proto.set_scope(registration->scope().spec());
  // Do not override name and icon if they are invalid.
  if (!app_info->name.empty()) {
    payment_app_proto.set_name(app_info->name);
  }
  if (!app_info->icon.empty()) {
    payment_app_proto.set_icon(app_info->icon);
  }
  payment_app_proto.set_prefer_related_applications(
      app_info->prefer_related_applications);
  for (const auto& related_app : app_info->related_applications) {
    StoredRelatedApplicationProto* related_app_proto =
        payment_app_proto.add_related_applications();
    related_app_proto->set_platform(related_app.platform);
    related_app_proto->set_id(related_app.id);
  }

  std::string serialized_payment_app;
  bool success = payment_app_proto.SerializeToString(&serialized_payment_app);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->key(),
      {{CreatePaymentAppKey(registration->scope().spec()),
        serialized_payment_app}},
      base::BindOnce(&PaymentAppDatabase::DidUpdatePaymentApp,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_info->name.empty() || app_info->icon.empty()));
}

void PaymentAppDatabase::DidUpdatePaymentApp(
    FetchAndUpdatePaymentAppInfoCallback callback,
    bool fetch_app_info_failed,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PaymentHandlerStatus handler_status =
      fetch_app_info_failed
          ? PaymentHandlerStatus::FETCH_PAYMENT_APP_INFO_FAILED
          : PaymentHandlerStatus::SUCCESS;
  handler_status = status == blink::ServiceWorkerStatusCode::kOk
                       ? handler_status
                       : PaymentHandlerStatus::STORAGE_OPERATION_FAILED;
  return std::move(callback).Run(handler_status);
}

void PaymentAppDatabase::ClearPaymentInstruments(
    const GURL& scope,
    ClearPaymentInstrumentsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40177656): Update this when PaymentManager
  // implements StorageKey.
  service_worker_context_->FindReadyRegistrationForScope(
      scope, blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)),
      base::BindOnce(
          &PaymentAppDatabase::DidFindRegistrationToClearPaymentInstruments,
          weak_ptr_factory_.GetWeakPtr(), scope, std::move(callback)));
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

  StoredPaymentInstrumentKeyInfoProto key_info_proto;
  key_info_proto.set_key(instrument_key);
  key_info_proto.set_insertion_order(base::Time::Now().ToInternalValue());

  std::string serialized_key_info;
  success = key_info_proto.SerializeToString(&serialized_key_info);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->key(),
      {{CreatePaymentInstrumentKey(instrument_key), serialized_instrument},
       {CreatePaymentInstrumentKeyInfoKey(instrument_key),
        serialized_key_info}},
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
    if (!base::Contains(apps, id))
      continue;

    apps[id]->enabled_methods.emplace_back(instrument_proto.method());
  }

  std::move(callback).Run(std::move(apps));
}

void PaymentAppDatabase::DidFindRegistrationToDeletePaymentInstrument(
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {CreatePaymentInstrumentKey(instrument_key)},
      base::BindOnce(&PaymentAppDatabase::DidFindPaymentInstrument,
                     weak_ptr_factory_.GetWeakPtr(), registration->id(),
                     instrument_key, std::move(callback)));
}

void PaymentAppDatabase::DidFindPaymentInstrument(
    int64_t registration_id,
    const std::string& instrument_key,
    DeletePaymentInstrumentCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk || data.size() != 1) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  service_worker_context_->ClearRegistrationUserData(
      registration_id,
      {CreatePaymentInstrumentKey(instrument_key),
       CreatePaymentInstrumentKeyInfoKey(instrument_key)},
      base::BindOnce(&PaymentAppDatabase::DidDeletePaymentInstrument,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidDeletePaymentInstrument(
    DeletePaymentInstrumentCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(callback).Run(status == blink::ServiceWorkerStatusCode::kOk
                                     ? PaymentHandlerStatus::SUCCESS
                                     : PaymentHandlerStatus::NOT_FOUND);
}

void PaymentAppDatabase::DidFindRegistrationToReadPaymentInstrument(
    const std::string& instrument_key,
    ReadPaymentInstrumentCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {CreatePaymentInstrumentKey(instrument_key)},
      base::BindOnce(&PaymentAppDatabase::DidReadPaymentInstrument,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidReadPaymentInstrument(
    ReadPaymentInstrumentCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk || data.size() != 1) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  PaymentInstrumentPtr instrument = ToPaymentInstrumentForMojo(data[0]);
  if (!instrument) {
    std::move(callback).Run(PaymentInstrument::New(),
                            PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
    return;
  }

  std::move(callback).Run(std::move(instrument), PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToGetKeys(
    KeysOfPaymentInstrumentsCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(std::vector<std::string>(),
                            PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserDataByKeyPrefix(
      registration->id(), {kPaymentInstrumentKeyInfoPrefix},
      base::BindOnce(&PaymentAppDatabase::DidGetKeysOfPaymentInstruments,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidGetKeysOfPaymentInstruments(
    KeysOfPaymentInstrumentsCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(std::vector<std::string>(),
                            PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  std::vector<std::string> keys;
  for (const auto& key_info : ToStoredPaymentInstrumentKeyInfos(data)) {
    keys.push_back(key_info.second);
  }

  std::move(callback).Run(keys, PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToHasPaymentInstrument(
    const std::string& instrument_key,
    HasPaymentInstrumentCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      registration->id(), {CreatePaymentInstrumentKey(instrument_key)},
      base::BindOnce(&PaymentAppDatabase::DidHasPaymentInstrument,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidHasPaymentInstrument(
    DeletePaymentInstrumentCallback callback,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk || data.size() != 1) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  std::move(callback).Run(PaymentHandlerStatus::SUCCESS);
}

void PaymentAppDatabase::DidFindRegistrationToWritePaymentInstrument(
    const std::string& instrument_key,
    PaymentInstrumentPtr instrument,
    const std::string& decoded_instrument_icon,
    WritePaymentInstrumentCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  StoredPaymentInstrumentProto instrument_proto;
  instrument_proto.set_registration_id(registration->id());
  instrument_proto.set_decoded_instrument_icon(decoded_instrument_icon);
  instrument_proto.set_instrument_key(instrument_key);
  instrument_proto.set_name(instrument->name);
  instrument_proto.set_method(instrument->method);
  for (const auto& icon : instrument->icons) {
    StoredPaymentInstrumentImageObject* image_object_proto =
        instrument_proto.add_icons();
    image_object_proto->set_src(icon.src.spec());
    image_object_proto->set_type(base::UTF16ToUTF8(icon.type));
    for (const auto& size : icon.sizes) {
      ImageSizeProto* size_proto = image_object_proto->add_sizes();
      size_proto->set_width(size.width());
      size_proto->set_height(size.height());
    }
  }

  std::string serialized_instrument;
  bool success = instrument_proto.SerializeToString(&serialized_instrument);
  DCHECK(success);

  StoredPaymentInstrumentKeyInfoProto key_info_proto;
  key_info_proto.set_key(instrument_key);
  key_info_proto.set_insertion_order(base::Time::Now().ToInternalValue());

  std::string serialized_key_info;
  success = key_info_proto.SerializeToString(&serialized_key_info);
  DCHECK(success);

  service_worker_context_->StoreRegistrationUserData(
      registration->id(), registration->key(),
      {{CreatePaymentInstrumentKey(instrument_key), serialized_instrument},
       {CreatePaymentInstrumentKeyInfoKey(instrument_key),
        serialized_key_info}},
      base::BindOnce(&PaymentAppDatabase::DidWritePaymentInstrument,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidWritePaymentInstrument(
    WritePaymentInstrumentCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(callback).Run(
      status == blink::ServiceWorkerStatusCode::kOk
          ? PaymentHandlerStatus::SUCCESS
          : PaymentHandlerStatus::STORAGE_OPERATION_FAILED);
}

void PaymentAppDatabase::DidFindRegistrationToClearPaymentInstruments(
    const GURL& scope,
    ClearPaymentInstrumentsCallback callback,
    blink::ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(PaymentHandlerStatus::NO_ACTIVE_WORKER);
    return;
  }

  KeysOfPaymentInstruments(
      scope,
      base::BindOnce(&PaymentAppDatabase::DidGetKeysToClearPaymentInstruments,
                     weak_ptr_factory_.GetWeakPtr(), std::move(registration),
                     std::move(callback)));
}

void PaymentAppDatabase::DidGetKeysToClearPaymentInstruments(
    scoped_refptr<ServiceWorkerRegistration> registration,
    ClearPaymentInstrumentsCallback callback,
    const std::vector<std::string>& keys,
    PaymentHandlerStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status != PaymentHandlerStatus::SUCCESS) {
    std::move(callback).Run(PaymentHandlerStatus::NOT_FOUND);
    return;
  }

  std::vector<std::string> keys_with_prefix;
  for (const auto& key : keys) {
    keys_with_prefix.push_back(CreatePaymentInstrumentKey(key));
    keys_with_prefix.push_back(CreatePaymentInstrumentKeyInfoKey(key));
  }

  // Clear payment app info after clearing all payment instruments.
  keys_with_prefix.push_back(CreatePaymentAppKey(registration->scope().spec()));

  service_worker_context_->ClearRegistrationUserData(
      registration->id(), keys_with_prefix,
      base::BindOnce(&PaymentAppDatabase::DidClearPaymentInstruments,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PaymentAppDatabase::DidClearPaymentInstruments(
    ClearPaymentInstrumentsCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(callback).Run(status == blink::ServiceWorkerStatusCode::kOk
                                     ? PaymentHandlerStatus::SUCCESS
                                     : PaymentHandlerStatus::NOT_FOUND);
}

}  // namespace content
