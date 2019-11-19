// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <math.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/child/child_thread_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/child_process.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/layout.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

#if defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#else
#include "content/child/webthemeengine_impl_default.h"
#endif

#if defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#endif

using blink::WebData;
using blink::WebString;
using blink::WebThemeEngine;
using blink::WebURL;
using blink::WebURLError;

namespace content {

namespace {

std::unique_ptr<blink::WebThemeEngine> GetWebThemeEngine() {
#if defined(OS_ANDROID)
  return std::make_unique<WebThemeEngineAndroid>();
#elif defined(OS_MACOSX)
  if (features::IsFormControlsRefreshEnabled())
    return std::make_unique<WebThemeEngineDefault>();
  return std::make_unique<WebThemeEngineMac>();
#else
  return std::make_unique<WebThemeEngineDefault>();
#endif
}

// This must match third_party/WebKit/public/blink_resources.grd.
// In particular, |is_gzipped| corresponds to compress="gzip".
struct DataResource {
  const char* name;
  int id;
  ui::ScaleFactor scale_factor;
  bool is_gzipped;
};

class NestedMessageLoopRunnerImpl
    : public blink::Platform::NestedMessageLoopRunner {
 public:
  NestedMessageLoopRunnerImpl() = default;

  ~NestedMessageLoopRunnerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Run() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::RunLoop* const previous_run_loop = run_loop_;
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = previous_run_loop;
  }

  void QuitNow() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(run_loop_);
    run_loop_->Quit();
  }

 private:
  base::RunLoop* run_loop_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

mojo::SharedRemote<mojom::ChildProcessHost> GetChildProcessHost() {
  auto* thread = ChildThreadImpl::current();
  if (thread)
    return thread->child_process_host();
  return {};
}

// An implementation of BrowserInterfaceBroker which forwards to the
// ChildProcessHost interface. This lives on the IO thread.
class ThreadSafeBrowserInterfaceBrokerProxyImpl
    : public blink::ThreadSafeBrowserInterfaceBrokerProxy {
 public:
  ThreadSafeBrowserInterfaceBrokerProxyImpl()
      : process_host_(GetChildProcessHost()) {}

  // blink::ThreadSafeBrowserInterfaceBrokerProxy implementation:
  void GetInterfaceImpl(mojo::GenericPendingReceiver receiver) override {
    if (process_host_)
      process_host_->BindHostReceiver(std::move(receiver));
  }

 private:
  ~ThreadSafeBrowserInterfaceBrokerProxyImpl() override = default;

  const mojo::SharedRemote<mojom::ChildProcessHost> process_host_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeBrowserInterfaceBrokerProxyImpl);
};

}  // namespace

// TODO(skyostil): Ensure that we always have an active task runner when
// constructing the platform.
BlinkPlatformImpl::BlinkPlatformImpl()
    : BlinkPlatformImpl(base::ThreadTaskRunnerHandle::IsSet()
                            ? base::ThreadTaskRunnerHandle::Get()
                            : nullptr,
                        nullptr) {}

BlinkPlatformImpl::BlinkPlatformImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
    : main_thread_task_runner_(std::move(main_thread_task_runner)),
      io_thread_task_runner_(std::move(io_thread_task_runner)),
      browser_interface_broker_proxy_(
          base::MakeRefCounted<ThreadSafeBrowserInterfaceBrokerProxyImpl>()),
      native_theme_engine_(GetWebThemeEngine()) {}

BlinkPlatformImpl::~BlinkPlatformImpl() = default;

void BlinkPlatformImpl::RecordAction(const blink::UserMetricsAction& name) {
  if (ChildThread* child_thread = ChildThread::Get())
    child_thread->RecordComputedAction(name.Action());
}

WebData BlinkPlatformImpl::GetDataResource(int resource_id,
                                           ui::ScaleFactor scale_factor) {
  base::StringPiece resource =
      GetContentClient()->GetDataResource(resource_id, scale_factor);
  return WebData(resource.data(), resource.size());
}

WebData BlinkPlatformImpl::UncompressDataResource(int resource_id) {
  base::StringPiece resource =
      GetContentClient()->GetDataResource(resource_id, ui::SCALE_FACTOR_NONE);
  if (resource.empty())
    return WebData(resource.data(), resource.size());
  std::string uncompressed;
  CHECK(compression::GzipUncompress(resource.as_string(), &uncompressed));
  return WebData(uncompressed.data(), uncompressed.size());
}

WebString BlinkPlatformImpl::QueryLocalizedString(int resource_id) {
  if (resource_id < 0)
    return WebString();
  return WebString::FromUTF16(
      GetContentClient()->GetLocalizedString(resource_id));
}

WebString BlinkPlatformImpl::QueryLocalizedString(int resource_id,
                                                  const WebString& value) {
  if (resource_id < 0)
    return WebString();

  base::string16 format_string =
      GetContentClient()->GetLocalizedString(resource_id);

  // If the ContentClient returned an empty string, e.g. because it's using the
  // default implementation of ContentClient::GetLocalizedString, return an
  // empty string instead of crashing with a failed DCHECK in
  // base::ReplaceStringPlaceholders below. This is useful for tests that don't
  // specialize a full ContentClient, since this way they can behave as though
  // there isn't a defined |resource_id| for the |name| instead of crashing
  // outright.
  if (format_string.empty())
    return WebString();

  return WebString::FromUTF16(
      base::ReplaceStringPlaceholders(format_string, value.Utf16(), nullptr));
}

WebString BlinkPlatformImpl::QueryLocalizedString(int resource_id,
                                                  const WebString& value1,
                                                  const WebString& value2) {
  if (resource_id < 0)
    return WebString();
  std::vector<base::string16> values;
  values.reserve(2);
  values.push_back(value1.Utf16());
  values.push_back(value2.Utf16());
  return WebString::FromUTF16(base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(resource_id), values, nullptr));
}

bool BlinkPlatformImpl::AllowScriptExtensionForServiceWorker(
    const blink::WebSecurityOrigin& script_origin) {
  return GetContentClient()->AllowScriptExtensionForServiceWorker(
      script_origin);
}

blink::WebCrypto* BlinkPlatformImpl::Crypto() {
  return &web_crypto_;
}

blink::ThreadSafeBrowserInterfaceBrokerProxy*
BlinkPlatformImpl::GetBrowserInterfaceBroker() {
  return browser_interface_broker_proxy_.get();
}

WebThemeEngine* BlinkPlatformImpl::ThemeEngine() {
  return native_theme_engine_.get();
}

bool BlinkPlatformImpl::IsURLSupportedForAppCache(const blink::WebURL& url) {
  return IsSchemeSupportedForAppCache(url);
}

size_t BlinkPlatformImpl::MaxDecodedImageBytes() {
  const int kMB = 1024 * 1024;
  const int kMaxNumberOfBytesPerPixel = 4;
#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    // Limit image decoded size to 3M pixels on low end devices.
    // 4 is maximum number of bytes per pixel.
    return 3 * kMB * kMaxNumberOfBytesPerPixel;
  }
  // For other devices, limit decoded image size based on the amount of physical
  // memory.
  // In some cases all physical memory is not accessible by Chromium, as it can
  // be reserved for direct use by certain hardware. Thus, we set the limit so
  // that 1.6GB of reported physical memory on a 2GB device is enough to set the
  // limit at 16M pixels, which is a desirable value since 4K*4K is a relatively
  // common texture size.
  return base::SysInfo::AmountOfPhysicalMemory() / 25;
#else
  size_t max_decoded_image_byte_limit = kNoDecodedImageByteLimit;
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kMaxDecodedImageSizeMb)) {
    if (base::StringToSizeT(
            command_line.GetSwitchValueASCII(switches::kMaxDecodedImageSizeMb),
            &max_decoded_image_byte_limit)) {
      max_decoded_image_byte_limit *= kMB * kMaxNumberOfBytesPerPixel;
    }
  }
  return max_decoded_image_byte_limit;
#endif
}

bool BlinkPlatformImpl::IsLowEndDevice() {
  return base::SysInfo::IsLowEndDevice();
}

scoped_refptr<base::SingleThreadTaskRunner> BlinkPlatformImpl::GetIOTaskRunner()
    const {
  return io_thread_task_runner_;
}

std::unique_ptr<blink::Platform::NestedMessageLoopRunner>
BlinkPlatformImpl::CreateNestedMessageLoopRunner() const {
  return std::make_unique<NestedMessageLoopRunnerImpl>();
}

}  // namespace content
