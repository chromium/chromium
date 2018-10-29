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
#include "media/blink/webmediacapabilitiesclient_impl.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_gesture_device.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/layout.h"

#if BUILDFLAG(USE_DEFAULT_RENDER_THEME)
#include "content/child/webthemeengine_impl_default.h"
#elif defined(OS_WIN)
#include "content/child/webthemeengine_impl_win.h"
#elif defined(OS_MACOSX)
#include "content/child/webthemeengine_impl_mac.h"
#elif defined(OS_ANDROID)
#include "content/child/webthemeengine_impl_android.h"
#endif

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
  blink::Platform::FileHandle DatabaseOpenFile(
      const blink::WebString& vfs_file_name,
      int desired_flags) override;
  int DatabaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;
  long DatabaseGetFileAttributes(
      const blink::WebString& vfs_file_name) override;
  long long DatabaseGetFileSize(const blink::WebString& vfs_file_name) override;
  long long DatabaseGetSpaceAvailableForOrigin(
      const blink::WebSecurityOrigin& origin) override;
  bool DatabaseSetFileSize(const blink::WebString& vfs_file_name,
                           long long size) override;

  size_t MaxDecodedImageBytes() override;
  bool IsLowEndDevice() override;
  void RecordAction(const blink::UserMetricsAction&) override;

  blink::WebData GetDataResource(const char* name) override;
  blink::WebString QueryLocalizedString(
      blink::WebLocalizedString::Name name) override;
  blink::WebString QueryLocalizedString(blink::WebLocalizedString::Name name,
                                        const blink::WebString& value) override;
  blink::WebString QueryLocalizedString(
      blink::WebLocalizedString::Name name,
      const blink::WebString& value1,
      const blink::WebString& value2) override;
  void SuddenTerminationChanged(bool enabled) override {}
  bool AllowScriptExtensionForServiceWorker(
      const blink::WebURL& script_url) override;
  blink::WebCrypto* Crypto() override;
  const char* GetBrowserServiceName() const override;
  blink::WebMediaCapabilitiesClient* MediaCapabilitiesClient() override;

  blink::WebString DomCodeStringFromEnum(int dom_code) override;
  int DomEnumFromCodeString(const blink::WebString& codeString) override;
  blink::WebString DomKeyStringFromEnum(int dom_key) override;
  int DomKeyEnumFromString(const blink::WebString& key_string) override;
  bool IsDomKeyForModifier(int dom_key) override;

  scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner() const override;
  std::unique_ptr<NestedMessageLoopRunner> CreateNestedMessageLoopRunner()
      const override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;
  WebThemeEngineImpl native_theme_engine_;
  webcrypto::WebCryptoImpl web_crypto_;
  media::WebMediaCapabilitiesClientImpl media_capabilities_client_;
};

}  // namespace content

#endif  // CONTENT_CHILD_BLINK_PLATFORM_IMPL_H_
