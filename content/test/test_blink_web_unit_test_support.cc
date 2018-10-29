// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_blink_web_unit_test_support.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_settings.h"
#include "content/app/mojo/mojo_init.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/loader/web_data_consumer_handle_impl.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/mojo/blink_interface_provider_impl.h"
#include "content/test/mock_clipboard_host.h"
#include "media/base/media.h"
#include "media/media_buildflags.h"
#include "net/cookies/cookie_monster.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/public/platform/web_connection_type.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_network_state_notifier.h"
#include "third_party/blink/public/platform/web_rtc_certificate_generator.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/public/web/blink.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"  // nogncheck
#endif

#include "third_party/webrtc/rtc_base/rtccertificate.h"  // nogncheck

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
    // This loader should be used only for process-local resources such as
    // data URLs.
    auto default_loader = std::make_unique<content::WebURLLoaderImpl>(
        nullptr, std::move(task_runner_handle), nullptr);
    return platform_->GetURLLoaderMockFactory()->CreateURLLoader(
        std::move(default_loader));
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

TestBlinkWebUnitTestSupport::TestBlinkWebUnitTestSupport()
    : weak_factory_(this) {
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

  url_loader_factory_ = blink::WebURLLoaderMockFactory::Create();
  // Mock out clipboard calls so that tests don't mess
  // with each other's copies/pastes when running in parallel.
  mock_clipboard_host_ = std::make_unique<MockClipboardHost>();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
  gin::V8Initializer::LoadV8Snapshot(kSnapshotType);
  gin::V8Initializer::LoadV8Natives();
#endif

  scoped_refptr<base::SingleThreadTaskRunner> dummy_task_runner;
  std::unique_ptr<base::ThreadTaskRunnerHandle> dummy_task_runner_handle;
  if (!base::ThreadTaskRunnerHandle::IsSet()) {
    // Dummy task runner is initialized here because the blink::Initialize
    // creates IsolateHolder which needs the current task runner handle. There
    // should be no task posted to this task runner. The message loop is not
    // created before this initialization because some tests need specific kinds
    // of message loops, and their types are not known upfront. Some tests also
    // create their own thread bundles or message loops, and doing the same in
    // TestBlinkWebUnitTestSupport would introduce a conflict.
    dummy_task_runner = base::MakeRefCounted<DummyTaskRunner>();
    dummy_task_runner_handle.reset(
        new base::ThreadTaskRunnerHandle(dummy_task_runner));
  }
  main_thread_scheduler_ =
      blink::scheduler::CreateWebMainThreadSchedulerForTests();

  // Initialize mojo firstly to enable Blink initialization to use it.
  InitializeMojo();

  connector_ = std::make_unique<service_manager::Connector>(
      service_manager::mojom::ConnectorPtrInfo());
  blink_interface_provider_.reset(
      new BlinkInterfaceProviderImpl(connector_.get()));

  service_manager::Connector::TestApi test_api(connector_.get());
  test_api.OverrideBinderForTesting(
      service_manager::Identity(mojom::kBrowserServiceName),
      blink::mojom::ClipboardHost::Name_,
      base::BindRepeating(&TestBlinkWebUnitTestSupport::BindClipboardHost,
                          weak_factory_.GetWeakPtr()));

  service_manager::BinderRegistry empty_registry;
  blink::Initialize(this, &empty_registry, main_thread_scheduler_.get());
  g_test_platform = this;
  blink::SetLayoutTestMode(true);
  blink::WebRuntimeFeatures::EnableDatabase(true);
  blink::WebRuntimeFeatures::EnableNotifications(true);
  blink::WebRuntimeFeatures::EnableTouchEventFeatureDetection(true);

  // Initialize NetworkStateNotifier.
  blink::WebNetworkStateNotifier::SetWebConnection(
      blink::WebConnectionType::kWebConnectionTypeUnknown,
      std::numeric_limits<double>::infinity());

  // Initialize libraries for media.
  media::InitializeMediaLibrary();

  if (!file_system_root_.CreateUniqueTempDir()) {
    LOG(WARNING) << "Failed to create a temp dir for the filesystem."
                    "FileSystem feature will be disabled.";
    DCHECK(file_system_root_.GetPath().empty());
  }

  // Test shell always exposes the GC.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
}

TestBlinkWebUnitTestSupport::~TestBlinkWebUnitTestSupport() {
  url_loader_factory_.reset();
  mock_clipboard_host_.reset();
  if (main_thread_scheduler_)
    main_thread_scheduler_->Shutdown();
  g_test_platform = nullptr;
}

blink::WebBlobRegistry* TestBlinkWebUnitTestSupport::GetBlobRegistry() {
  return &blob_registry_;
}

std::unique_ptr<blink::WebURLLoaderFactory>
TestBlinkWebUnitTestSupport::CreateDefaultURLLoaderFactory() {
  return std::make_unique<WebURLLoaderFactoryWithMock>(
      weak_factory_.GetWeakPtr());
}

std::unique_ptr<blink::WebDataConsumerHandle>
TestBlinkWebUnitTestSupport::CreateDataConsumerHandle(
    mojo::ScopedDataPipeConsumerHandle handle) {
  return std::make_unique<WebDataConsumerHandleImpl>(std::move(handle));
}

blink::WebString TestBlinkWebUnitTestSupport::UserAgent() {
  return blink::WebString::FromUTF8("test_runner/0.0.0.0");
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    blink::WebLocalizedString::Name name) {
  // Returns placeholder strings to check if they are correctly localized.
  switch (name) {
    case blink::WebLocalizedString::kFileButtonNoFileSelectedLabel:
      return WebString::FromASCII("<<NoFileChosenLabel>>");
    case blink::WebLocalizedString::kOtherDateLabel:
      return WebString::FromASCII("<<OtherDateLabel>>");
    case blink::WebLocalizedString::kOtherMonthLabel:
      return WebString::FromASCII("<<OtherMonthLabel>>");
    case blink::WebLocalizedString::kOtherWeekLabel:
      return WebString::FromASCII("<<OtherWeekLabel>>");
    case blink::WebLocalizedString::kCalendarClear:
      return WebString::FromASCII("<<CalendarClear>>");
    case blink::WebLocalizedString::kCalendarToday:
      return WebString::FromASCII("<<CalendarToday>>");
    case blink::WebLocalizedString::kThisMonthButtonLabel:
      return WebString::FromASCII("<<ThisMonthLabel>>");
    case blink::WebLocalizedString::kThisWeekButtonLabel:
      return WebString::FromASCII("<<ThisWeekLabel>>");
    case blink::WebLocalizedString::kValidationValueMissing:
      return WebString::FromASCII("<<ValidationValueMissing>>");
    case blink::WebLocalizedString::kValidationValueMissingForSelect:
      return WebString::FromASCII("<<ValidationValueMissingForSelect>>");
    case blink::WebLocalizedString::kWeekFormatTemplate:
      return WebString::FromASCII("Week $2, $1");
    default:
      return blink::WebString();
  }
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value) {
  if (name == blink::WebLocalizedString::kValidationRangeUnderflow)
    return blink::WebString::FromASCII("range underflow");
  if (name == blink::WebLocalizedString::kValidationRangeOverflow)
    return blink::WebString::FromASCII("range overflow");
  if (name == blink::WebLocalizedString::kSelectMenuListText)
    return blink::WebString::FromASCII("$1 selected");
  return BlinkPlatformImpl::QueryLocalizedString(name, value);
}

blink::WebString TestBlinkWebUnitTestSupport::QueryLocalizedString(
    blink::WebLocalizedString::Name name,
    const blink::WebString& value1,
    const blink::WebString& value2) {
  if (name == blink::WebLocalizedString::kValidationTooLong)
    return blink::WebString::FromASCII("too long");
  if (name == blink::WebLocalizedString::kValidationStepMismatch)
    return blink::WebString::FromASCII("step mismatch");
  return BlinkPlatformImpl::QueryLocalizedString(name, value1, value2);
}

blink::WebString TestBlinkWebUnitTestSupport::DefaultLocale() {
  return blink::WebString::FromASCII("en-US");
}

blink::WebURLLoaderMockFactory*
TestBlinkWebUnitTestSupport::GetURLLoaderMockFactory() {
  return url_loader_factory_.get();
}

bool TestBlinkWebUnitTestSupport::IsThreadedAnimationEnabled() {
  return threaded_animation_;
}

namespace {

class TestWebRTCCertificateGenerator
    : public blink::WebRTCCertificateGenerator {
  void GenerateCertificate(
      const blink::WebRTCKeyParams& key_params,
      std::unique_ptr<blink::WebRTCCertificateCallback> callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    NOTIMPLEMENTED();
  }
  void GenerateCertificateWithExpiration(
      const blink::WebRTCKeyParams& key_params,
      uint64_t expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    NOTIMPLEMENTED();
  }
  bool IsSupportedKeyParams(const blink::WebRTCKeyParams& key_params) override {
    return false;
  }
  rtc::scoped_refptr<rtc::RTCCertificate> FromPEM(
      blink::WebString pem_private_key,
      blink::WebString pem_certificate) override {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::FromPEM(rtc::RTCCertificatePEM(
            pem_private_key.Utf8(), pem_certificate.Utf8()));
    return certificate;
  }
};

}  // namespace

std::unique_ptr<blink::WebRTCCertificateGenerator>
TestBlinkWebUnitTestSupport::CreateRTCCertificateGenerator() {
  return std::make_unique<TestWebRTCCertificateGenerator>();
}

service_manager::Connector* TestBlinkWebUnitTestSupport::GetConnector() {
  return connector_.get();
}

blink::InterfaceProvider* TestBlinkWebUnitTestSupport::GetInterfaceProvider() {
  return blink_interface_provider_.get();
}

void TestBlinkWebUnitTestSupport::BindClipboardHost(
    mojo::ScopedMessagePipeHandle handle) {
  mock_clipboard_host_->Bind(
      blink::mojom::ClipboardHostRequest(std::move(handle)));
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
