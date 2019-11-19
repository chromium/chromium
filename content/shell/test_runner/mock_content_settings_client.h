// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_CONTENT_SETTINGS_CLIENT_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_CONTENT_SETTINGS_CLIENT_H_

#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/common/client_hints.mojom.h"
#include "content/shell/utility/mock_client_hints_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "url/origin.h"

namespace test_runner {

class WebTestDelegate;
class WebTestRuntimeFlags;

class MockContentSettingsClient : public blink::WebContentSettingsClient {
 public:
  // Caller has to guarantee that |layout_test_runtime_flags| lives longer
  // than the MockContentSettingsClient being constructed here.
  MockContentSettingsClient(WebTestRuntimeFlags* layout_test_runtime_flags);

  ~MockContentSettingsClient() override;

  // blink::WebContentSettingsClient:
  bool AllowImage(bool enabled_per_settings,
                  const blink::WebURL& image_url) override;
  bool AllowScript(bool enabled_per_settings) override;
  bool AllowScriptFromSource(bool enabled_per_settings,
                             const blink::WebURL& script_url) override;
  bool AllowStorage(bool local) override;
  bool AllowRunningInsecureContent(bool enabled_per_settings,
                                   const blink::WebURL& url) override;
  bool AllowAutoplay(bool default_value) override;
  void PersistClientHints(
      const blink::WebEnabledClientHints& enabled_client_hints,
      base::TimeDelta duration,
      const blink::WebURL& url) override;
  void GetAllowedClientHintsFromSource(
      const blink::WebURL& url,
      blink::WebEnabledClientHints* client_hints) const override;

  void SetDelegate(WebTestDelegate* delegate);

  void ResetClientHintsPersistencyData();

 private:
  WebTestDelegate* delegate_;

  WebTestRuntimeFlags* flags_;
  mojo::Remote<client_hints::mojom::ClientHints> remote_;

  content::ClientHintsContainer client_hints_map_;

  DISALLOW_COPY_AND_ASSIGN(MockContentSettingsClient);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_CONTENT_SETTINGS_CLIENT_H_
