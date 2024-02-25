// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_gesture_device.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/public_buildflags.h"

namespace webcrypto {
class WebCryptoImpl;
}  // namespace webcrypto

namespace content {

class CONTENT_EXPORT BlinkPlatformImpl : public blink::Platform {
 public:
  BlinkPlatformImpl();
  explicit BlinkPlatformImpl(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);
  ~BlinkPlatformImpl() override;

  // blink::Platform implementation.
  bool IsURLSavableForSavableResource(const blink::WebURL& url) override;
  size_t MaxDecodedImageBytes() override;
  bool IsLowEndDevice() override;
  void RecordAction(const blink::UserMetricsAction&) override;
  blink::WebData GetDataResource(int resource_id,
                                 ui::ResourceScaleFactor scale_factor) override;
  std::string GetDataResourceString(int resource_id) override;
  blink::WebString QueryLocalizedString(int resource_id) override;
  blink::WebString QueryLocalizedString(int resource_id,
                                        const blink::WebString& value) override;
  blink::WebString QueryLocalizedString(
      int resource_id,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  void SuddenTerminationChanged(bool enabled) override {}
  blink::WebCrypto* Crypto() override;
  blink::ThreadSafeBrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker()
      override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const override;
  scoped_refptr<base::SequencedTaskRunner>
  GetMediaStreamVideoSourceVideoTaskRunner() const override;
  std::unique_ptr<NestedMessageLoopRunner> CreateNestedMessageLoopRunner()
      const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner>
      media_stream_video_source_video_task_runner_;
  const scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_proxy_;
  webcrypto::WebCryptoImpl web_crypto_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
