// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_blink_web_unit_test_support.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_settings.h"
#include "content/child/child_process.h"
#include "media/base/media.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/cookies/cookie_monster.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/blink.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "v8/include/v8.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"  // nogncheck
#endif

#include "third_party/webrtc/rtc_base/rtc_certificate.h"  // nogncheck

using blink::WebString;

namespace {

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8SnapshotFileType kSnapshotType =
    gin::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8SnapshotFileType kSnapshotType =
    gin::V8SnapshotFileType::kDefault;
#endif
#endif

content::TestBlinkWebUnitTestSupport* g_test_platform = nullptr;

}  // namespace

namespace content {

TestBlinkWebUnitTestSupport::TestBlinkWebUnitTestSupport(
    SchedulerType scheduler_type,
    std::string additional_v8_flags) {
#if BUILDFLAG(IS_APPLE)
  base::apple::ScopedNSAutoreleasePool autorelease_pool;
#endif

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif

  // Test shell always exposes the GC, and some tests need to modify flags so do
  // not freeze them on initialization.
  std::string v8_flags("--expose-gc --no-freeze-flags-after-init");
  v8_flags += additional_v8_flags;

  blink::Platform::InitializeBlink();
  scoped_refptr<base::SingleThreadTaskRunner> dummy_task_runner;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle>
      dummy_task_runner_handle;
  if (scheduler_type == SchedulerType::kMockScheduler) {
    main_thread_scheduler_ =
        blink::scheduler::CreateWebMainThreadSchedulerForTests();
    // Dummy task runner is initialized here because the blink::Initialize
    // creates IsolateHolder which needs the current task runner handle. There
    // should be no task posted to this task runner. The message loop is not
    // created before this initialization because some tests need specific kinds
    // of message loops, and their types are not known upfront. Some tests also
    // create their own thread bundles or message loops, and doing the same in
    // TestBlinkWebUnitTestSupport would introduce a conflict.
    dummy_task_runner = base::MakeRefCounted<base::NullTaskRunner>();
    dummy_task_runner_handle =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            dummy_task_runner);
  } else {
    DCHECK_EQ(scheduler_type, SchedulerType::kRealScheduler);
    main_thread_scheduler_ =
        blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler(
            base::MessagePump::Create(base::MessagePumpType::DEFAULT));
    base::test::TaskEnvironment::CreateThreadPool();
    base::ThreadPoolInstance::Get()->StartWithDefaultParams();
  }

  // Set V8 flags.
  v8::V8::SetFlagsFromString(v8_flags.c_str(), v8_flags.size());

  // Makes Mojo calls to the browser. This is called inside
  // blink::Initialize so it needs to be set first.
  blink::WebRuntimeFeatures::EnableAndroidDownloadableFontsMatching(false);

  mojo::BinderMap binders;
  blink::InitializeWithoutIsolateForTesting(this, &binders,
                                            main_thread_scheduler_.get());
  g_test_platform = this;
  blink::SetWebTestMode(true);
  blink::WebRuntimeFeatures::EnableNotifications(true);
  blink::WebRuntimeFeatures::EnableTouchEventFeatureDetection(true);

  // Initialize NetworkStateNotifier.
  blink::WebNetworkStateNotifier::SetWebConnection(
      blink::WebConnectionType::kWebConnectionTypeUnknown,
      std::numeric_limits<double>::infinity());

  // Initialize libraries for media.
  media::InitializeMediaLibrary();
}

TestBlinkWebUnitTestSupport::~TestBlinkWebUnitTestSupport() {
  if (main_thread_scheduler_)
    main_thread_scheduler_->Shutdown();
  g_test_platform = nullptr;
}

blink::WebString TestBlinkWebUnitTestSupport::UserAgent() {
  return blink::WebString::FromUTF8("test_runner/0.0.0.0");
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    int resource_id) {
  // Returns placeholder strings to check if they are correctly localized.
  switch (resource_id) {
    case IDS_FORM_FILE_NO_FILE_LABEL:
      return WebString::FromASCII("<<NoFileChosenLabel>>");
    case IDS_FORM_OTHER_DATE_LABEL:
      return WebString::FromASCII("<<OtherDate>>");
    case IDS_FORM_OTHER_MONTH_LABEL:
      return WebString::FromASCII("<<OtherMonth>>");
    case IDS_FORM_OTHER_WEEK_LABEL:
      return WebString::FromASCII("<<OtherWeek>>");
    case IDS_FORM_CALENDAR_CLEAR:
      return WebString::FromASCII("<<Clear>>");
    case IDS_FORM_CALENDAR_TODAY:
      return WebString::FromASCII("<<Today>>");
    case IDS_FORM_THIS_MONTH_LABEL:
      return WebString::FromASCII("<<ThisMonth>>");
    case IDS_FORM_THIS_WEEK_LABEL:
      return WebString::FromASCII("<<ThisWeek>>");
    case IDS_FORM_VALIDATION_VALUE_MISSING:
      return WebString::FromASCII("<<ValidationValueMissing>>");
    case IDS_FORM_VALIDATION_VALUE_MISSING_SELECT:
      return WebString::FromASCII("<<ValidationValueMissingForSelect>>");
    case IDS_FORM_INPUT_WEEK_TEMPLATE:
      return WebString::FromASCII("Week $2, $1");
    default:
      return blink::WebString();
  }
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    int resource_id,
    const blink::WebString& value) {
  switch (resource_id) {
    case IDS_FORM_VALIDATION_RANGE_UNDERFLOW:
      return blink::WebString::FromASCII("range underflow");
    case IDS_FORM_VALIDATION_RANGE_OVERFLOW:
      return blink::WebString::FromASCII("range overflow");
    case IDS_FORM_SELECT_MENU_LIST_TEXT:
      return blink::WebString::FromASCII(value.Ascii() + " selected");
  }

  return BlinkPlatformImpl::QueryLocalizedString(resource_id, value);
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    int resource_id,
    const blink::WebString& value1,
    const blink::WebString& value2) {
  switch (resource_id) {
    case IDS_FORM_VALIDATION_TOO_LONG:
      return blink::WebString::FromASCII("too long");
    case IDS_FORM_VALIDATION_STEP_MISMATCH:
      return blink::WebString::FromASCII("step mismatch");
  }

  return BlinkPlatformImpl::QueryLocalizedString(resource_id, value1, value2);
}

blink::WebString TestBlinkWebUnitTestSupport::DefaultLocale() {
  return blink::WebString::FromASCII("en-US");
}

scoped_refptr<base::SingleThreadTaskRunner>
TestBlinkWebUnitTestSupport::GetIOTaskRunner() const {
  return ChildProcess::current() ? ChildProcess::current()->io_task_runner()
                                 : nullptr;
}

bool TestBlinkWebUnitTestSupport::IsThreadedAnimationEnabled() {
  return threaded_animation_;
}

// static
bool TestBlinkWebUnitTestSupport::SetThreadedAnimationEnabled(bool enabled) {
  DCHECK(g_test_platform)
      << "Not using TestBlinkWebUnitTestSupport as blink::Platform";
  bool old = g_test_platform->threaded_animation_;
  g_test_platform->threaded_animation_ = enabled;
  return old;
}

}  // namespace content
