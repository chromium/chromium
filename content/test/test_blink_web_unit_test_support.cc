// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_blink_web_unit_test_support.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/null_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_settings.h"
#include "content/app/mojo/mojo_init.h"
#include "content/child/child_process.h"
#include "content/public/common/service_names.mojom.h"
#include "content/test/mock_clipboard_host.h"
#include "media/base/media.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/cookies/cookie_monster.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"  // nogncheck
#endif

#include "third_party/webrtc/rtc_base/rtc_certificate.h"  // nogncheck

using blink::WebString;

namespace {

class DummyTaskRunner : public base::SingleThreadTaskRunner {
 public:
  DummyTaskRunner() : thread_id_(base::PlatformThread::CurrentId()) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    // Drop the delayed task.
    return false;
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    // Drop the delayed task.
    return false;
  }

  bool RunsTasksInCurrentSequence() const override {
    return thread_id_ == base::PlatformThread::CurrentId();
  }

 protected:
  ~DummyTaskRunner() override {}

  base::PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskRunner);
};

// TODO(kinuko,toyoshim): Deprecate this, all Blink tests should not rely
// on this //content implementation.
class WebURLLoaderFactoryWithMock : public blink::WebURLLoaderFactory {
 public:
  explicit WebURLLoaderFactoryWithMock(base::WeakPtr<blink::Platform> platform)
      : platform_(std::move(platform)) {}
  ~WebURLLoaderFactoryWithMock() override = default;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
          task_runner_handle) override {
    DCHECK(platform_);
    return platform_->GetURLLoaderMockFactory()->CreateURLLoader();
  }

 private:
  base::WeakPtr<blink::Platform> platform_;
  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderFactoryWithMock);
};

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
#if defined(USE_V8_CONTEXT_SNAPSHOT)
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kWithAdditionalContext;
#else
constexpr gin::V8Initializer::V8SnapshotFileType kSnapshotType =
    gin::V8Initializer::V8SnapshotFileType::kDefault;
#endif
#endif

content::TestBlinkWebUnitTestSupport* g_test_platform = nullptr;

}  // namespace

namespace content {

TestBlinkWebUnitTestSupport::TestBlinkWebUnitTestSupport(
    TestBlinkWebUnitTestSupport::SchedulerType scheduler_type) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  url_loader_factory_ = blink::WebURLLoaderMockFactory::Create();
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  mock_clipboard_host_ = std::make_unique<MockClipboardHost>();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
#endif

  scoped_refptr<base::SingleThreadTaskRunner> dummy_task_runner;
  std::unique_ptr<base::ThreadTaskRunnerHandle> dummy_task_runner_handle;
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
    dummy_task_runner_handle.reset(
        new base::ThreadTaskRunnerHandle(dummy_task_runner));
  } else {
    DCHECK_EQ(scheduler_type, SchedulerType::kRealScheduler);
    main_thread_scheduler_ =
        blink::scheduler::WebThreadScheduler::CreateMainThreadScheduler(
            base::MessagePump::Create(base::MessagePumpType::DEFAULT));
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
        "BlinkTestSupport");
  }

  // Initialize mojo firstly to enable Blink initialization to use it.
  InitializeMojo();

  mojo::BinderMap binders;
  blink::Initialize(this, &binders, main_thread_scheduler_.get());
  g_test_platform = this;
  blink::SetWebTestMode(true);
  blink::WebRuntimeFeatures::EnableDatabase(true);
  blink::WebRuntimeFeatures::EnableNotifications(true);
  blink::WebRuntimeFeatures::EnableTouchEventFeatureDetection(true);

  // Initialize NetworkStateNotifier.
  blink::WebNetworkStateNotifier::SetWebConnection(
      blink::WebConnectionType::kWebConnectionTypeUnknown,
      std::numeric_limits<double>::infinity());

  // Initialize libraries for media.
  media::InitializeMediaLibrary();

  // Test shell always exposes the GC.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), flags.size());

  GetBrowserInterfaceBroker()->SetBinderForTesting(
      blink::mojom::ClipboardHost::Name_,
      base::BindRepeating(&TestBlinkWebUnitTestSupport::BindClipboardHost,
                          weak_factory_.GetWeakPtr()));
}

TestBlinkWebUnitTestSupport::~TestBlinkWebUnitTestSupport() {
  url_loader_factory_.reset();
  mock_clipboard_host_.reset();
  if (main_thread_scheduler_)
    main_thread_scheduler_->Shutdown();
  g_test_platform = nullptr;
}

std::unique_ptr<blink::WebURLLoaderFactory>
TestBlinkWebUnitTestSupport::CreateDefaultURLLoaderFactory() {
  return std::make_unique<WebURLLoaderFactoryWithMock>(
      weak_factory_.GetWeakPtr());
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
      return WebString::FromASCII("<<OtherDateLabel>>");
    case IDS_FORM_OTHER_MONTH_LABEL:
      return WebString::FromASCII("<<OtherMonthLabel>>");
    case IDS_FORM_OTHER_WEEK_LABEL:
      return WebString::FromASCII("<<OtherWeekLabel>>");
    case IDS_FORM_CALENDAR_CLEAR:
      return WebString::FromASCII("<<CalendarClear>>");
    case IDS_FORM_CALENDAR_TODAY:
      return WebString::FromASCII("<<CalendarToday>>");
    case IDS_FORM_THIS_MONTH_LABEL:
      return WebString::FromASCII("<<ThisMonthLabel>>");
    case IDS_FORM_THIS_WEEK_LABEL:
      return WebString::FromASCII("<<ThisWeekLabel>>");
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
      return blink::WebString::FromASCII("$1 selected");
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

blink::WebURLLoaderMockFactory*
TestBlinkWebUnitTestSupport::GetURLLoaderMockFactory() {
  return url_loader_factory_.get();
}

bool TestBlinkWebUnitTestSupport::IsThreadedAnimationEnabled() {
  return threaded_animation_;
}

void TestBlinkWebUnitTestSupport::BindClipboardHost(
    mojo::ScopedMessagePipeHandle handle) {
  mock_clipboard_host_->Bind(
      mojo::PendingReceiver<blink::mojom::ClipboardHost>(std::move(handle)));
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
