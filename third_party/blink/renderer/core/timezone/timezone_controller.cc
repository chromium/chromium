// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timezone/timezone_controller.h"

#include "base/feature_list.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/icu/source/common/unicode/char16ptr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// When enabled, the host timezone id is evaluated only when needed.
// TODO(crbug.com/40287434): Cleanup the feature after running the experiment,
// no later than January 2025.
BASE_FEATURE(kLazyBlinkTimezoneInit,
             "LazyBlinkTimezoneInit",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Notify V8 that the date/time configuration of the system might have changed.
void NotifyTimezoneChangeToV8(v8::Isolate* isolate) {
  DCHECK(isolate);
  isolate->DateTimeConfigurationChangeNotification();
}

void NotifyTimezoneChangeOnWorkerThread(WorkerThread* worker_thread) {
  DCHECK(worker_thread->IsCurrentThread());
  NotifyTimezoneChangeToV8(worker_thread->GlobalScope()->GetIsolate());
  if (RuntimeEnabledFeatures::TimeZoneChangeEventEnabled() &&
      worker_thread->GlobalScope()->IsWorkerGlobalScope()) {
    worker_thread->GlobalScope()->DispatchEvent(
        *Event::Create(event_type_names::kTimezonechange));
  }
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

void DispatchTimeZoneChangeEventToFrames() {
  if (!RuntimeEnabledFeatures::TimeZoneChangeEventEnabled())
    return;

  for (const Page* page : Page::OrdinaryPages()) {
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (auto* main_local_frame = DynamicTo<LocalFrame>(frame)) {
        main_local_frame->DomWindow()->EnqueueWindowEvent(
            *Event::Create(event_type_names::kTimezonechange),
            TaskType::kMiscPlatformAPI);
      }
    }
  }
}

bool SetIcuTimeZoneAndNotifyV8(const String& timezone_id) {
  DCHECK(!timezone_id.empty());
  std::unique_ptr<icu::TimeZone> timezone(icu::TimeZone::createTimeZone(
      icu::UnicodeString(timezone_id.Ascii().data(), -1, US_INV)));
  CHECK(timezone);

  if (*timezone == icu::TimeZone::getUnknown())
    return false;

  icu::TimeZone::adoptDefault(timezone.release());

  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(WTF::BindRepeating(
          [](v8::Isolate* isolate) { NotifyTimezoneChangeToV8(isolate); }));
  WorkerThread::CallOnAllWorkerThreads(&NotifyTimezoneChangeOnWorkerThread,
                                       TaskType::kInternalDefault);
  DispatchTimeZoneChangeEventToFrames();
  return true;
}

}  // namespace

TimeZoneController::TimeZoneController() {
  DCHECK(IsMainThread());
  if (!base::FeatureList::IsEnabled(kLazyBlinkTimezoneInit)) {
    host_timezone_id_ = GetCurrentTimezoneId();
  }
}

TimeZoneController::~TimeZoneController() = default;

// static
void TimeZoneController::Init() {
  // monitor must not use HeapMojoRemote. TimeZoneController is not managed by
  // Oilpan. monitor is only used to bind receiver_ here and never used
  // again.
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

bool CanonicalEquals(const String& time_zone_a, const String& time_zone_b) {
  if (time_zone_a == time_zone_b) {
    return true;
  }
  icu::UnicodeString canonical_a, canonical_b;
  UErrorCode status = U_ZERO_ERROR;
  UBool dummy;
  icu::TimeZone::getCanonicalID(
      icu::UnicodeString(time_zone_a.Ascii().data(), -1, US_INV), canonical_a,
      dummy, status);
  icu::TimeZone::getCanonicalID(
      icu::UnicodeString(time_zone_b.Ascii().data(), -1, US_INV), canonical_b,
      dummy, status);
  if (U_FAILURE(status)) {
    return false;
  }
  return canonical_a == canonical_b;
}

// static
std::unique_ptr<TimeZoneController::TimeZoneOverride>
TimeZoneController::SetTimeZoneOverride(const String& timezone_id) {
  DCHECK(!timezone_id.empty());
  if (HasTimeZoneOverride()) {
    VLOG(1) << "Cannot override existing timezone override.";
    return nullptr;
  }

  // Only notify if the override and the host are different.
  if (!CanonicalEquals(timezone_id, instance().GetHostTimezoneId())) {
    if (!SetIcuTimeZoneAndNotifyV8(timezone_id)) {
      VLOG(1) << "Invalid override timezone id: " << timezone_id;
      return nullptr;
    }
  }
  instance().override_timezone_id_ = timezone_id;

  return std::unique_ptr<TimeZoneOverride>(new TimeZoneOverride());
}

// static
bool TimeZoneController::HasTimeZoneOverride() {
  return !instance().override_timezone_id_.empty();
}

// static
const String& TimeZoneController::TimeZoneIdOverride() {
  return instance().override_timezone_id_;
}

// static
void TimeZoneController::ClearTimeZoneOverride() {
  DCHECK(HasTimeZoneOverride());

  if (!CanonicalEquals(instance().GetHostTimezoneId(),
                       instance().override_timezone_id_)) {
    // Restore remembered timezone request.
    // Only do so if the host timezone is now different.
    SetIcuTimeZoneAndNotifyV8(instance().GetHostTimezoneId());
  }
  instance().override_timezone_id_ = String();
}

// static
void TimeZoneController::ChangeTimeZoneOverride(const String& timezone_id) {
  DCHECK(!timezone_id.empty());
  if (!HasTimeZoneOverride()) {
    VLOG(1) << "Cannot change if there are no existing timezone override.";
    return;
  }

  if (CanonicalEquals(instance().override_timezone_id_, timezone_id)) {
    return;
  }

  if (!SetIcuTimeZoneAndNotifyV8(timezone_id)) {
    VLOG(1) << "Invalid override timezone id: " << timezone_id;
    return;
  }
  instance().override_timezone_id_ = timezone_id;
}
void TimeZoneController::OnTimeZoneChange(const String& timezone_id) {
  DCHECK(IsMainThread());

  // Remember requested timezone id so we can set it when timezone
  // override is removed.
  instance().host_timezone_id_ = timezone_id;

  if (!HasTimeZoneOverride())
    SetIcuTimeZoneAndNotifyV8(timezone_id);
}

const String& TimeZoneController::GetHostTimezoneId() {
  if (!host_timezone_id_.has_value()) {
    CHECK(base::FeatureList::IsEnabled(kLazyBlinkTimezoneInit));
    host_timezone_id_ = GetCurrentTimezoneId();
  }
  return host_timezone_id_.value();
}

// static
void TimeZoneController::ChangeTimeZoneForTesting(const String& timezone) {
  instance().OnTimeZoneChange(timezone);
}
}  // namespace blink
