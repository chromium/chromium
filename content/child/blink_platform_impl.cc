// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <math.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/child/child_thread_impl.h"
#include "content/common/child_process.mojom.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/user_metrics_action.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"

using blink::WebData;
using blink::WebString;
using blink::WebURL;
using blink::WebURLError;

namespace content {
namespace {

// This must match third_party/WebKit/public/blink_resources.grd.
struct DataResource {
  const char* name;
  int id;
  ui::ResourceScaleFactor scale_factor;
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
  raw_ptr<base::RunLoop> run_loop_ = nullptr;

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

  ThreadSafeBrowserInterfaceBrokerProxyImpl(
      const ThreadSafeBrowserInterfaceBrokerProxyImpl&) = delete;
  ThreadSafeBrowserInterfaceBrokerProxyImpl& operator=(
      const ThreadSafeBrowserInterfaceBrokerProxyImpl&) = delete;

  // blink::ThreadSafeBrowserInterfaceBrokerProxy implementation:
  void GetInterfaceImpl(mojo::GenericPendingReceiver receiver) override {
    if (process_host_)
      process_host_->BindHostReceiver(std::move(receiver));
  }

 private:
  ~ThreadSafeBrowserInterfaceBrokerProxyImpl() override = default;

  const mojo::SharedRemote<mojom::ChildProcessHost> process_host_;
};

}  // namespace

// TODO(skyostil): Ensure that we always have an active task runner when
// constructing the platform.
BlinkPlatformImpl::BlinkPlatformImpl() : BlinkPlatformImpl(nullptr) {}

BlinkPlatformImpl::BlinkPlatformImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
    : io_thread_task_runner_(std::move(io_thread_task_runner)),
      media_stream_video_source_video_task_runner_(
          base::FeatureList::IsEnabled(
              blink::features::kUseThreadPoolForMediaStreamVideoTaskRunner)
              ? base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits{})
              : io_thread_task_runner_),
      browser_interface_broker_proxy_(
          base::MakeRefCounted<ThreadSafeBrowserInterfaceBrokerProxyImpl>()) {}

BlinkPlatformImpl::~BlinkPlatformImpl() = default;

void BlinkPlatformImpl::RecordAction(const blink::UserMetricsAction& name) {
  if (ChildThread* child_thread = ChildThread::Get())
    child_thread->RecordComputedAction(name.Action());
}

WebData BlinkPlatformImpl::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  std::string_view resource =
      GetContentClient()->GetDataResource(resource_id, scale_factor);
  return WebData(resource.data(), resource.size());
}

std::string BlinkPlatformImpl::GetDataResourceString(int resource_id) {
  return GetContentClient()->GetDataResourceString(resource_id);
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

  std::u16string format_string =
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
  std::vector<std::u16string> values;
  values.reserve(2);
  values.push_back(value1.Utf16());
  values.push_back(value2.Utf16());
  return WebString::FromUTF16(base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(resource_id), values, nullptr));
}

blink::WebCrypto* BlinkPlatformImpl::Crypto() {
  return &web_crypto_;
}

blink::ThreadSafeBrowserInterfaceBrokerProxy*
BlinkPlatformImpl::GetBrowserInterfaceBroker() {
  return browser_interface_broker_proxy_.get();
}

bool BlinkPlatformImpl::IsURLSavableForSavableResource(
    const blink::WebURL& url) {
  return IsSavableURL(url);
}

size_t BlinkPlatformImpl::MaxDecodedImageBytes() {
  const int kMB = 1024 * 1024;
  const int kMaxNumberOfBytesPerPixel = 4;
#if BUILDFLAG(IS_ANDROID)
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
  // This value is static for performance because calculating it is non-trivial.
  static bool is_low_end_device = base::SysInfo::IsLowEndDevice();
  return is_low_end_device;
}

scoped_refptr<base::SingleThreadTaskRunner> BlinkPlatformImpl::GetIOTaskRunner()
    const {
  return io_thread_task_runner_;
}

scoped_refptr<base::SequencedTaskRunner>
BlinkPlatformImpl::GetMediaStreamVideoSourceVideoTaskRunner() const {
  return media_stream_video_source_video_task_runner_;
}

std::unique_ptr<blink::Platform::NestedMessageLoopRunner>
BlinkPlatformImpl::CreateNestedMessageLoopRunner() const {
  return std::make_unique<NestedMessageLoopRunnerImpl>();
}

}  // namespace content
