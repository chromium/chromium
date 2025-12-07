// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/locale_controller.h"

#include "base/i18n/rtl.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
void UpdateDefaultLocaleInIsolate(v8::Isolate* isolate) {
  DCHECK(isolate);
  isolate->LocaleConfigurationChangeNotification();
  isolate->DateTimeConfigurationChangeNotification();
}

void UpdateDefaultLocaleInMainIsolates() {
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(&UpdateDefaultLocaleInIsolate);
}

void NotifyLocaleChangeOnWorkerThread(WorkerThread* worker_thread) {
  DCHECK(worker_thread->IsCurrentThread());
  UpdateDefaultLocaleInIsolate(worker_thread->GlobalScope()->GetIsolate());
}

}  // namespace

LocaleController::LocaleController()
    : embedder_locale_(String(icu::Locale::getDefault().getName())) {}

String LocaleController::SetLocaleOverride(const String& locale,
                                           bool is_claiming_override) {
  base::AutoLock locker(lock_);

  // Only allow resetting overrides set by the same agent.
  if (is_claiming_override && !locale_override_.empty()) {
    return "Another locale override is already in effect";
  }

  if (locale_override_ == locale)
    return String();
  if (locale.empty()) {
    UpdateLocale(embedder_locale_);
  } else {
    icu::Locale locale_object(locale.Ascii().data());
    const char* lang = locale_object.getLanguage();
    if (!lang || *lang == '\0')
      return "Invalid locale name";
    UpdateLocale(locale);
  }
  locale_override_ = locale;
  return String();
}

void LocaleController::UpdateLocale(const String& locale) {
  WebString web_locale(locale);
  base::i18n::SetICUDefaultLocale(web_locale.Ascii());
  if (IsMainThread()) {
    UpdateDefaultLocaleInMainIsolates();
  } else {
    Thread::MainThread()
        ->GetTaskRunner(MainThreadTaskRunnerRestricted())
        ->PostTask(FROM_HERE, ConvertToBaseOnceCallback(CrossThreadBindOnce(
                                  &UpdateDefaultLocaleInMainIsolates)));
  }
  WorkerThread::CallOnAllWorkerThreads(&NotifyLocaleChangeOnWorkerThread,
                                       TaskType::kInternalDefault);
}

// static
LocaleController& LocaleController::instance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(LocaleController, instance, ());
  return instance;
}

}  // namespace blink
