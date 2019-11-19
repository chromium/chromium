// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
#define CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/webcrypto/webcrypto_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_gesture_device.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/layout.h"

namespace content {

class WebCryptoImpl;

class CONTENT_EXPORT BlinkPlatformImpl : public blink::Platform {
 public:
  BlinkPlatformImpl();
  explicit BlinkPlatformImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner);
  ~BlinkPlatformImpl() override;

  // Platform methods (partial implementation):
  blink::WebThemeEngine* ThemeEngine() override;
  bool IsURLSupportedForAppCache(const blink::WebURL& url) override;

  size_t MaxDecodedImageBytes() override;
  bool IsLowEndDevice() override;
  void RecordAction(const blink::UserMetricsAction&) override;

  blink::WebData GetDataResource(int resource_id,
                                 ui::ScaleFactor scale_factor) override;
  blink::WebData UncompressDataResource(int resource_id) override;
  blink::WebString QueryLocalizedString(int resource_id) override;
  blink::WebString QueryLocalizedString(int resource_id,
                                        const blink::WebString& value) override;
  blink::WebString QueryLocalizedString(
      int resource_id,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  void SuddenTerminationChanged(bool enabled) override {}
  bool AllowScriptExtensionForServiceWorker(
      const blink::WebSecurityOrigin& script_origin) override;
  blink::WebCrypto* Crypto() override;
  blink::ThreadSafeBrowserInterfaceBrokerProxy* GetBrowserInterfaceBroker()
      override;

  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const override;
  std::unique_ptr<NestedMessageLoopRunner> CreateNestedMessageLoopRunner()
      const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;
  const scoped_refptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      browser_interface_broker_proxy_;
  std::unique_ptr<blink::WebThemeEngine> native_theme_engine_;
  webcrypto::WebCryptoImpl web_crypto_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
