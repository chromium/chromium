// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_SETTINGS_CLIENT_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_SETTINGS_CLIENT_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "url/origin.h"

namespace content {

class TestRunner;
class WebTestRuntimeFlags;

class WebTestContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  // The |test_runner| and |layout_test_runtime_flags| must outlive this class.
  WebTestContentSettingsClient(TestRunner* test_runner,
                               WebTestRuntimeFlags* layout_test_runtime_flags);

  ~WebTestContentSettingsClient() override;

  WebTestContentSettingsClient(const WebTestContentSettingsClient&) = delete;
  WebTestContentSettingsClient& operator=(const WebTestContentSettingsClient&) =
      delete;

  // blink::WebContentSettingsClient:
  bool AllowImage(bool enabled_per_settings,
                  const blink::WebURL& image_url) override;
  bool AllowScript(bool enabled_per_settings) override;
  bool AllowScriptFromSource(bool enabled_per_settings,
                             const blink::WebURL& script_url) override;
  bool AllowStorageAccessSync(StorageType storage_type) override;
  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const blink::WebURL& url) override;
  bool IncreaseViewTransitionCallbackTimeout() const override;

 private:
  TestRunner* const test_runner_;
  WebTestRuntimeFlags* const flags_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_SETTINGS_CLIENT_H_
