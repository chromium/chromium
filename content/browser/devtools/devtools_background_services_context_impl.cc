// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_background_services_context_impl.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

namespace {

std::string CreateEntryKeyPrefix(devtools::proto::BackgroundService service) {
  DCHECK_NE(service, devtools::proto::BackgroundService::UNKNOWN);
  return "devtools_background_services_" + base::NumberToString(service) + "_";
}

std::string CreateEntryKey(devtools::proto::BackgroundService service) {
  return CreateEntryKeyPrefix(service) +
         base::Uuid::GenerateRandomV4().AsLowercaseString();
}

constexpr devtools::proto::BackgroundService ServiceToProtoEnum(
    DevToolsBackgroundService service) {
  switch (service) {
    case DevToolsBackgroundService::kBackgroundFetch:
      return devtools::proto::BACKGROUND_FETCH;
    case DevToolsBackgroundService::kBackgroundSync:
      return devtools::proto::BACKGROUND_SYNC;
    case DevToolsBackgroundService::kPushMessaging:
      return devtools::proto::PUSH_MESSAGING;
    case DevToolsBackgroundService::kNotifications:
      return devtools::proto::NOTIFICATIONS;
    case DevToolsBackgroundService::kPaymentHandler:
      return devtools::proto::PAYMENT_HANDLER;
    case DevToolsBackgroundService::kPeriodicBackgroundSync:
      return devtools::proto::PERIODIC_BACKGROUND_SYNC;
  }
}

}  // namespace

DevToolsBackgroundServicesContextImpl::DevToolsBackgroundServicesContextImpl(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : browser_context_(CHECK_DEREF(browser_context)),
      service_worker_context_(std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto expiration_times =
      GetContentClient()->browser()->GetDevToolsBackgroundServiceExpirations(
          &*browser_context_);

  for (const auto& expiration_time : expiration_times) {
    DCHECK(devtools::proto::BackgroundService_IsValid(expiration_time.first));
    expiration_times_[expiration_time.first] = expiration_time.second;

    auto service =
        static_cast<devtools::proto::BackgroundService>(expiration_time.first);
    // If the recording permission for |service| has expired, set it to null.
    if (IsRecordingExpired(service))
      expiration_times_[expiration_time.first] = base::Time();
  }
}

DevToolsBackgroundServicesContextImpl::
    ~DevToolsBackgroundServicesContextImpl() = default;

void DevToolsBackgroundServicesContextImpl::AddObserver(
    EventObserver* observer) {
  observers_.AddObserver(observer);
}

void DevToolsBackgroundServicesContextImpl::RemoveObserver(
    const EventObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DevToolsBackgroundServicesContextImpl::StartRecording(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(rayankans): Make the time delay finch configurable.
  base::Time expiration_time = base::Time::Now() + base::Days(3);
  expiration_times_[service] = expiration_time;

  GetContentClient()->browser()->UpdateDevToolsBackgroundServiceExpiration(
      &*browser_context_, service, expiration_time);

  for (EventObserver& observer : observers_)
    observer.OnRecordingStateChanged(/* should_record= */ true, service);
}

void DevToolsBackgroundServicesContextImpl::StopRecording(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  expiration_times_[service] = base::Time();
  GetContentClient()->browser()->UpdateDevToolsBackgroundServiceExpiration(
      &*browser_context_, service, base::Time());

  for (EventObserver& observer : observers_)
    observer.OnRecordingStateChanged(/* should_record= */ false, service);
}

bool DevToolsBackgroundServicesContextImpl::IsRecording(
    DevToolsBackgroundService service) {
  return IsRecording(ServiceToProtoEnum(service));
}

bool DevToolsBackgroundServicesContextImpl::IsRecording(
    devtools::proto::BackgroundService service) {
  // Returns whether |service| has been enabled. When the expiration time has
  // been met it will be lazily updated to be null.
  return !expiration_times_[service].is_null();
}

bool DevToolsBackgroundServicesContextImpl::IsRecordingExpired(
    devtools::proto::BackgroundService service) {
  // Copy the expiration time to avoid data races.
  const base::Time expiration_time = expiration_times_[service];
  return !expiration_time.is_null() && expiration_time < base::Time::Now();
}

void DevToolsBackgroundServicesContextImpl::GetLoggedBackgroundServiceEvents(
    devtools::proto::BackgroundService service,
    GetLoggedBackgroundServiceEventsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context_->GetUserDataForAllRegistrationsByKeyPrefix(
      CreateEntryKeyPrefix(service),
      base::BindOnce(&DevToolsBackgroundServicesContextImpl::DidGetUserData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DevToolsBackgroundServicesContextImpl::DidGetUserData(
    GetLoggedBackgroundServiceEventsCallback callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<devtools::proto::BackgroundServiceEvent> events;

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(events);
    return;
  }

  events.reserve(user_data.size());
  for (const auto& data : user_data) {
    devtools::proto::BackgroundServiceEvent event;
    if (!event.ParseFromString(data.second)) {
      // TODO(rayankans): Log errors to UMA.
      std::move(callback).Run({});
      return;
    }
    DCHECK_EQ(data.first, event.service_worker_registration_id());
    events.push_back(std::move(event));
  }

  std::sort(events.begin(), events.end(),
            [](const auto& state1, const auto& state2) {
              return state1.timestamp() < state2.timestamp();
            });

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(events)));
}

void DevToolsBackgroundServicesContextImpl::ClearLoggedBackgroundServiceEvents(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_worker_context_->ClearUserDataForAllRegistrationsByKeyPrefix(
      CreateEntryKeyPrefix(service), base::DoNothing());
}

void DevToolsBackgroundServicesContextImpl::LogBackgroundServiceEvent(
    uint64_t service_worker_registration_id,
    blink::StorageKey storage_key,
    DevToolsBackgroundService service,
    const std::string& event_name,
    const std::string& instance_id,
    const std::map<std::string, std::string>& event_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsRecording(service))
    return;

  if (IsRecordingExpired(ServiceToProtoEnum(service))) {
    // We should stop recording because of the expiration time. We should
    // also inform the observers that we stopped recording.
    OnRecordingTimeExpired(ServiceToProtoEnum(service));
    return;
  }

  devtools::proto::BackgroundServiceEvent event;
  event.set_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  event.set_origin(storage_key.origin().GetURL().spec());
  event.set_storage_key(storage_key.Serialize());
  event.set_service_worker_registration_id(service_worker_registration_id);
  event.set_background_service(ServiceToProtoEnum(service));
  event.set_event_name(event_name);
  event.set_instance_id(instance_id);
  event.mutable_event_metadata()->insert(event_metadata.begin(),
                                         event_metadata.end());

  service_worker_context_->StoreRegistrationUserData(
      service_worker_registration_id, storage_key,
      {{CreateEntryKey(event.background_service()), event.SerializeAsString()}},
      base::DoNothing());

  NotifyEventObservers(std::move(event));
}

void DevToolsBackgroundServicesContextImpl::NotifyEventObservers(
    const devtools::proto::BackgroundServiceEvent& event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (EventObserver& observer : observers_)
    observer.OnEventReceived(event);
}

void DevToolsBackgroundServicesContextImpl::OnRecordingTimeExpired(
    devtools::proto::BackgroundService service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // This could have been stopped by the user in the meanwhile, or we
  // received duplicate time expiry events.
  if (IsRecordingExpired(service))
    StopRecording(service);
}

}  // namespace content
