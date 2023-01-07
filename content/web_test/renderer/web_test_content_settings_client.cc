// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_test_content_settings_client.h"

#include "content/public/common/origin_util.h"
#include "content/web_test/common/web_test_runtime_flags.h"
#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/renderer/test_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"

namespace content {

WebTestContentSettingsClient::WebTestContentSettingsClient(
    TestRunner* test_runner,
    WebTestRuntimeFlags* web_test_runtime_flags)
    : test_runner_(test_runner), flags_(web_test_runtime_flags) {}

WebTestContentSettingsClient::~WebTestContentSettingsClient() = default;

bool WebTestContentSettingsClient::AllowImage(bool enabled_per_settings,
                                              const blink::WebURL& image_url) {
  bool allowed = enabled_per_settings && flags_->images_allowed();
  if (flags_->dump_web_content_settings_client_callbacks()) {
    test_runner_->PrintMessage(
        std::string("WebTestContentSettingsClient: allowImage(") +
        web_test_string_util::NormalizeWebTestURLForTextOutput(
            image_url.GetString().Utf8()) +
        "): " + (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool WebTestContentSettingsClient::AllowScript(bool enabled_per_settings) {
  return enabled_per_settings && flags_->scripts_allowed();
}

bool WebTestContentSettingsClient::AllowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  bool allowed = enabled_per_settings && flags_->scripts_allowed();
  if (flags_->dump_web_content_settings_client_callbacks()) {
    test_runner_->PrintMessage(
        std::string("WebTestContentSettingsClient: allowScriptFromSource(") +
        web_test_string_util::NormalizeWebTestURLForTextOutput(
            script_url.GetString().Utf8()) +
        "): " + (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool WebTestContentSettingsClient::AllowStorageAccessSync(
    StorageType storage_type) {
  return flags_->storage_allowed();
}

bool WebTestContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebURL& url) {
  return enabled_per_settings || flags_->running_insecure_content_allowed();
}

bool WebTestContentSettingsClient::
    IncreaseSharedElementTransitionCallbackTimeout() const {
  // In tests we want larger timeout to account for slower running tests.
  return true;
}

}  // namespace content
