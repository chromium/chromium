// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/mock_content_settings_client.h"

#include "content/public/common/origin_util.h"
#include "content/shell/test_runner/test_common.h"
#include "content/shell/test_runner/web_test_delegate.h"
#include "content/shell/test_runner/web_test_runtime_flags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"

namespace test_runner {

MockContentSettingsClient::MockContentSettingsClient(
    WebTestRuntimeFlags* web_test_runtime_flags)
    : delegate_(nullptr), flags_(web_test_runtime_flags) {
  mojo::PendingRemote<client_hints::mojom::ClientHints> host_observer;
  blink::Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      host_observer.InitWithNewPipeAndPassReceiver());
  remote_.Bind(std::move(host_observer));
}

MockContentSettingsClient::~MockContentSettingsClient() {}

bool MockContentSettingsClient::AllowImage(bool enabled_per_settings,
                                           const blink::WebURL& image_url) {
  bool allowed = enabled_per_settings && flags_->images_allowed();
  if (flags_->dump_web_content_settings_client_callbacks() && delegate_) {
    delegate_->PrintMessage(
        std::string("MockContentSettingsClient: allowImage(") +
        NormalizeWebTestURL(image_url.GetString().Utf8()) +
        "): " + (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool MockContentSettingsClient::AllowScript(bool enabled_per_settings) {
  return enabled_per_settings && flags_->scripts_allowed();
}

bool MockContentSettingsClient::AllowScriptFromSource(
    bool enabled_per_settings,
    const blink::WebURL& script_url) {
  bool allowed = enabled_per_settings && flags_->scripts_allowed();
  if (flags_->dump_web_content_settings_client_callbacks() && delegate_) {
    delegate_->PrintMessage(
        std::string("MockContentSettingsClient: allowScriptFromSource(") +
        NormalizeWebTestURL(script_url.GetString().Utf8()) +
        "): " + (allowed ? "true" : "false") + "\n");
  }
  return allowed;
}

bool MockContentSettingsClient::AllowStorage(bool enabled_per_settings) {
  return flags_->storage_allowed();
}

bool MockContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const blink::WebURL& url) {
  return enabled_per_settings || flags_->running_insecure_content_allowed();
}

bool MockContentSettingsClient::AllowAutoplay(bool default_value) {
  return flags_->autoplay_allowed();
}

void MockContentSettingsClient::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
}

namespace {

void ConvertWebEnabledClientHintsToWebClientHintsTypeVector(
    const blink::WebEnabledClientHints& enabled_client_hints,
    const int max_length,
    std::vector<blink::mojom::WebClientHintsType>* client_hints) {
  DCHECK(client_hints);
  for (int type = 0; type < max_length; ++type) {
    blink::mojom::WebClientHintsType client_hints_type =
        static_cast<blink::mojom::WebClientHintsType>(type);
    if (enabled_client_hints.IsEnabled(client_hints_type)) {
      client_hints->push_back(client_hints_type);
    }
  }
}

void PersistClientHintsInEmbedder(
    const blink::WebEnabledClientHints& enabled_client_hints,
    base::TimeDelta duration,
    const blink::WebURL& url,
    const mojo::Remote<client_hints::mojom::ClientHints>& remote) {
  const int max_length =
      static_cast<int>(blink::mojom::WebClientHintsType::kMaxValue) + 1;
  std::vector<blink::mojom::WebClientHintsType> client_hints(max_length);
  const url::Origin origin = url::Origin::Create(url);

  ConvertWebEnabledClientHintsToWebClientHintsTypeVector(
      enabled_client_hints, max_length, &client_hints);

  remote->PersistClientHints(origin, client_hints, duration);
}
}  // namespace

void MockContentSettingsClient::PersistClientHints(
    const blink::WebEnabledClientHints& enabled_client_hints,
    base::TimeDelta duration,
    const blink::WebURL& url) {
  if (!PersistClientHintsHelper(url, enabled_client_hints, duration,
                                &client_hints_map_)) {
    return;
  }

  PersistClientHintsInEmbedder(enabled_client_hints, duration, url, remote_);
}

void MockContentSettingsClient::GetAllowedClientHintsFromSource(
    const blink::WebURL& url,
    blink::WebEnabledClientHints* client_hints) const {
  GetAllowedClientHintsFromSourceHelper(url, client_hints_map_, client_hints);
}

void MockContentSettingsClient::ResetClientHintsPersistencyData() {
  client_hints_map_.clear();
}

}  // namespace test_runner
