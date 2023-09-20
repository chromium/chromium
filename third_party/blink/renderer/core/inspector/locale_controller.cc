// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/locale_controller.h"

#include "base/i18n/rtl.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "v8/include/v8.h"

namespace blink {

namespace {
void UpdateDefaultLocaleInIsolate(v8::Isolate* isolate) {
  DCHECK(isolate);
  isolate->LocaleConfigurationChangeNotification();
  isolate->DateTimeConfigurationChangeNotification();
}

void NotifyLocaleChangeOnWorkerThread(WorkerThread* worker_thread) {
  DCHECK(worker_thread->IsCurrentThread());
  UpdateDefaultLocaleInIsolate(worker_thread->GlobalScope()->GetIsolate());
}

void UpdateLocale(const String& locale) {
  WebString web_locale(locale);
  base::i18n::SetICUDefaultLocale(web_locale.Ascii());
  Thread::MainThread()
      ->Scheduler()
      ->ToMainThreadScheduler()
      ->ForEachMainThreadIsolate(
          WTF::BindRepeating(&UpdateDefaultLocaleInIsolate));
  WorkerThread::CallOnAllWorkerThreads(&NotifyLocaleChangeOnWorkerThread,
                                       TaskType::kInternalDefault);
}
}  // namespace

LocaleController::LocaleController()
    : embedder_locale_(String(icu::Locale::getDefault().getName())) {}

String LocaleController::SetLocaleOverride(const String& locale) {
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

bool LocaleController::has_locale_override() const {
  return !locale_override_.empty();
}

// static
LocaleController& LocaleController::instance() {
  DEFINE_STATIC_LOCAL(LocaleController, instance, ());
  return instance;
}

}  // namespace blink
