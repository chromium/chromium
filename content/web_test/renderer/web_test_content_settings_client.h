// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_SETTINGS_CLIENT_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_SETTINGS_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "url/origin.h"

namespace content {

class TestRunner;
class WebFrameTestProxy;

class WebTestContentSettingsClient : public RenderFrameObserver,
                                     public blink::WebContentSettingsClient {
 public:
  // The lifecycle of this object is tied to the lifecycle of the provided
  // `frame`. `test_runner` must outlive this class.
  WebTestContentSettingsClient(WebFrameTestProxy* frame,
                               TestRunner* test_runner);

  ~WebTestContentSettingsClient() override;

  WebTestContentSettingsClient(const WebTestContentSettingsClient&) = delete;
  WebTestContentSettingsClient& operator=(const WebTestContentSettingsClient&) =
      delete;

  // RenderFrameObserver:
  void OnDestruct() override;

  // blink::WebContentSettingsClient:
  bool AllowStorageAccessSync(StorageType storage_type) override;
  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const blink::WebURL& url) override;

 private:
  const raw_ptr<TestRunner> test_runner_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_SETTINGS_CLIENT_H_
