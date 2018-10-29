// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/time_zone_monitor/time_zone_monitor_client.h"

#include "services/device/public/mojom/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Notify V8 that the date/time configuration of the system might have changed.
void NotifyTimezoneChangeToV8(v8::Isolate* isolate) {
  DCHECK(isolate);
  v8::Date::DateTimeConfigurationChangeNotification(isolate);
}

void NotifyTimezoneChangeOnWorkerThread(WorkerThread* worker_thread) {
  DCHECK(worker_thread->IsCurrentThread());
  NotifyTimezoneChangeToV8(ToIsolate(worker_thread->GlobalScope()));
}

}  // namespace

// static
void TimeZoneMonitorClient::Init() {
  DEFINE_STATIC_LOCAL(TimeZoneMonitorClient, instance, ());

  device::mojom::blink::TimeZoneMonitorPtr monitor;
  Platform::Current()->GetConnector()->BindInterface(
      device::mojom::blink::kServiceName, mojo::MakeRequest(&monitor));
  device::mojom::blink::TimeZoneMonitorClientPtr client;
  instance.binding_.Bind(mojo::MakeRequest(&client));
  monitor->AddClient(std::move(client));
}

TimeZoneMonitorClient::TimeZoneMonitorClient() : binding_(this) {
  DCHECK(IsMainThread());
}

TimeZoneMonitorClient::~TimeZoneMonitorClient() = default;

void TimeZoneMonitorClient::OnTimeZoneChange(const String& time_zone_info) {
  DCHECK(IsMainThread());

  if (!time_zone_info.IsEmpty()) {
    DCHECK(time_zone_info.ContainsOnlyASCIIOrEmpty());
    icu::TimeZone* zone = icu::TimeZone::createTimeZone(
        icu::UnicodeString(time_zone_info.Ascii().data(), -1, US_INV));
    icu::TimeZone::adoptDefault(zone);
    VLOG(1) << "ICU default timezone is set to " << time_zone_info;
  }

  NotifyTimezoneChangeToV8(V8PerIsolateData::MainThreadIsolate());
  WorkerThread::CallOnAllWorkerThreads(&NotifyTimezoneChangeOnWorkerThread);
}

}  // namespace blink
