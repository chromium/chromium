// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"

#include <utility>

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_type_converters.h"

namespace blink {

// static
BackgroundFetchBridge* BackgroundFetchBridge::From(
    ServiceWorkerRegistration* service_worker_registration) {
  DCHECK(service_worker_registration);

  BackgroundFetchBridge* bridge =
      Supplement<ServiceWorkerRegistration>::From<BackgroundFetchBridge>(
          service_worker_registration);

  if (!bridge) {
    bridge = MakeGarbageCollected<BackgroundFetchBridge>(
        *service_worker_registration);
    ProvideTo(*service_worker_registration, bridge);
  }

  return bridge;
}

// static
const char BackgroundFetchBridge::kSupplementName[] = "BackgroundFetchBridge";

BackgroundFetchBridge::BackgroundFetchBridge(
    ServiceWorkerRegistration& registration)
    : Supplement<ServiceWorkerRegistration>(registration),
      background_fetch_service_(registration.GetExecutionContext()) {}

BackgroundFetchBridge::~BackgroundFetchBridge() = default;

void BackgroundFetchBridge::Trace(Visitor* visitor) const {
  visitor->Trace(background_fetch_service_);
  Supplement::Trace(visitor);
}

void BackgroundFetchBridge::GetIconDisplaySize(
    GetIconDisplaySizeCallback callback) {
  GetService()->GetIconDisplaySize(std::move(callback));
}

void BackgroundFetchBridge::Fetch(
    const String& developer_id,
    Vector<mojom::blink::FetchAPIRequestPtr> requests,
    mojom::blink::BackgroundFetchOptionsPtr options,
    const SkBitmap& icon,
    mojom::blink::BackgroundFetchUkmDataPtr ukm_data,
    RegistrationCallback callback) {
  GetService()->Fetch(GetSupplementable()->RegistrationId(), developer_id,
                      std::move(requests), std::move(options), icon,
                      std::move(ukm_data),
                      WTF::BindOnce(&BackgroundFetchBridge::DidGetRegistration,
                                    WrapPersistent(this), std::move(callback)));
}

void BackgroundFetchBridge::GetRegistration(const String& developer_id,
                                            RegistrationCallback callback) {
  GetService()->GetRegistration(
      GetSupplementable()->RegistrationId(), developer_id,
      WTF::BindOnce(&BackgroundFetchBridge::DidGetRegistration,
                    WrapPersistent(this), std::move(callback)));
}

void BackgroundFetchBridge::DidGetRegistration(
    RegistrationCallback callback,
    mojom::blink::BackgroundFetchError error,
    mojom::blink::BackgroundFetchRegistrationPtr registration_ptr) {
  if (!registration_ptr || !registration_ptr->registration_data) {
    DCHECK_NE(error, mojom::blink::BackgroundFetchError::NONE);
    std::move(callback).Run(error, nullptr);
    return;
  }

  DCHECK_EQ(error, mojom::blink::BackgroundFetchError::NONE);
  BackgroundFetchRegistration* registration =
      MakeGarbageCollected<blink::BackgroundFetchRegistration>(
          GetSupplementable(), std::move(registration_ptr));

  std::move(callback).Run(error, registration);
}

void BackgroundFetchBridge::GetDeveloperIds(GetDeveloperIdsCallback callback) {
  GetService()->GetDeveloperIds(GetSupplementable()->RegistrationId(),
                                std::move(callback));
}

mojom::blink::BackgroundFetchService* BackgroundFetchBridge::GetService() {
  if (!background_fetch_service_.is_bound()) {
    auto receiver = background_fetch_service_.BindNewPipeAndPassReceiver(
        GetSupplementable()->GetExecutionContext()->GetTaskRunner(
            TaskType::kBackgroundFetch));
    GetSupplementable()
        ->GetExecutionContext()
        ->GetBrowserInterfaceBroker()
        .GetInterface(std::move(receiver));
  }
  return background_fetch_service_.get();
}

}  // namespace blink
