// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_bridge.h"

#include <utility>

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_options.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_type_converters.h"
#include "third_party/blink/renderer/modules/manifest/image_resource.h"

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
    : Supplement<ServiceWorkerRegistration>(registration) {}

BackgroundFetchBridge::~BackgroundFetchBridge() = default;

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
  GetService()->Fetch(
      GetSupplementable()->RegistrationId(), developer_id, std::move(requests),
      std::move(options), icon, std::move(ukm_data),
      WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                WrapPersistent(this), WTF::Passed(std::move(callback))));
}

void BackgroundFetchBridge::GetRegistration(const String& developer_id,
                                            RegistrationCallback callback) {
  GetService()->GetRegistration(
      GetSupplementable()->RegistrationId(), developer_id,
      WTF::Bind(&BackgroundFetchBridge::DidGetRegistration,
                WrapPersistent(this), WTF::Passed(std::move(callback))));
}

void BackgroundFetchBridge::DidGetRegistration(
    RegistrationCallback callback,
    mojom::blink::BackgroundFetchError error,
    mojom::blink::BackgroundFetchRegistrationPtr registration_ptr) {
  BackgroundFetchRegistration* registration =
      registration_ptr.To<BackgroundFetchRegistration*>();

  if (registration) {
    DCHECK_EQ(error, mojom::blink::BackgroundFetchError::NONE);
    DCHECK_EQ(registration->result(), "");
    registration->Initialize(
        GetSupplementable(),
        std::move(registration_ptr->registration_interface));
  }

  std::move(callback).Run(error, registration);
}

void BackgroundFetchBridge::GetDeveloperIds(GetDeveloperIdsCallback callback) {
  GetService()->GetDeveloperIds(GetSupplementable()->RegistrationId(),
                                std::move(callback));
}

mojom::blink::BackgroundFetchService* BackgroundFetchBridge::GetService() {
  if (!background_fetch_service_) {
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
