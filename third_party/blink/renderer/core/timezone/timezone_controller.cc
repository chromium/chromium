// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timezone/timezone_controller.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"
#include "third_party/icu/source/common/unicode/char16ptr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Notify V8 that the date/time configuration of the system might have changed.
void NotifyTimezoneChangeToV8(v8::Isolate* isolate) {
  DCHECK(isolate);
  isolate->DateTimeConfigurationChangeNotification();
}

void NotifyTimezoneChangeOnWorkerThread(WorkerThread* worker_thread) {
  DCHECK(worker_thread->IsCurrentThread());
  NotifyTimezoneChangeToV8(worker_thread->GlobalScope()->GetIsolate());
}

String GetTimezoneId(const icu::TimeZone& timezone) {
  icu::UnicodeString unicode_timezone_id;
  timezone.getID(unicode_timezone_id);
  return String(icu::toUCharPtr(unicode_timezone_id.getBuffer()),
                static_cast<unsigned>(unicode_timezone_id.length()));
}

String GetCurrentTimezoneId() {
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createDefault());
  CHECK(timezone);
  return GetTimezoneId(*timezone.get());
}

bool SetIcuTimeZoneAndNotifyV8(const String& timezone_id) {
  DCHECK(!timezone_id.IsEmpty());
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id.Ascii().data(), -1, US_INV)));
  CHECK(timezone);

  if (*timezone == icu::TimeZone::getUnknown())
    return false;

  icu::TimeZone::adoptDefault(timezone.release());

  NotifyTimezoneChangeToV8(V8PerIsolateData::MainThreadIsolate());
  WorkerThread::CallOnAllWorkerThreads(&NotifyTimezoneChangeOnWorkerThread,
                                       TaskType::kInternalDefault);
  return true;
}

}  // namespace

TimeZoneController::TimeZoneController() {
  DCHECK(IsMainThread());
  host_timezone_id_ = GetCurrentTimezoneId();
}

TimeZoneController::~TimeZoneController() = default;

// static
void TimeZoneController::Init() {
  // Some unit tests may have no message loop ready, so we can't initialize the
  // mojo stuff here. They can initialize those mojo stuff they're interested in
  // later after they got a message loop ready.
  if (!CanInitializeMojo())
    return;

  mojo::Remote<device::mojom::blink::TimeZoneMonitor> monitor;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      monitor.BindNewPipeAndPassReceiver());
  monitor->AddClient(instance().receiver_.BindNewPipeAndPassRemote());
}

// static
TimeZoneController& TimeZoneController::instance() {
  DEFINE_STATIC_LOCAL(TimeZoneController, instance, ());
  return instance;
}

// static
std::unique_ptr<TimeZoneController::TimeZoneOverride>
TimeZoneController::SetTimeZoneOverride(const String& timezone_id) {
  DCHECK(!timezone_id.IsEmpty());
  if (instance().has_timezone_id_override_) {
    VLOG(1) << "Cannot override existing timezone override.";
    return nullptr;
  }

  if (!SetIcuTimeZoneAndNotifyV8(timezone_id)) {
    VLOG(1) << "Invalid override timezone id: " << timezone_id;
    return nullptr;
  }

  instance().has_timezone_id_override_ = true;

  return std::unique_ptr<TimeZoneOverride>(new TimeZoneOverride());
}

// static
bool TimeZoneController::HasTimeZoneOverride() {
  return instance().has_timezone_id_override_;
}

// static
void TimeZoneController::ClearTimeZoneOverride() {
  DCHECK(instance().has_timezone_id_override_);

  instance().has_timezone_id_override_ = false;

  // Restore remembered timezone request.
  SetIcuTimeZoneAndNotifyV8(instance().host_timezone_id_);
}

void TimeZoneController::OnTimeZoneChange(const String& timezone_id) {
  DCHECK(IsMainThread());

  // Remember requested timezone id so we can set it when timezone
  // override is removed.
  instance().host_timezone_id_ = timezone_id;

  if (!instance().has_timezone_id_override_)
    SetIcuTimeZoneAndNotifyV8(timezone_id);
}

}  // namespace blink
