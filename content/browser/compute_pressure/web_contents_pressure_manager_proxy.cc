// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/unguessable_token.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom.h"

namespace content {

WebContentsPressureManagerProxy::WebContentsPressureManagerProxy(
    WebContents* web_contents)
    : WebContentsUserData<WebContentsPressureManagerProxy>(*web_contents) {}

WebContentsPressureManagerProxy::~WebContentsPressureManagerProxy() = default;

// static
WebContentsPressureManagerProxy* WebContentsPressureManagerProxy::GetOrCreate(
    WebContents* web_contents) {
  WebContentsUserData<WebContentsPressureManagerProxy>::CreateForWebContents(
      web_contents);
  return WebContentsUserData<WebContentsPressureManagerProxy>::FromWebContents(
      web_contents);
}

std::unique_ptr<ScopedVirtualPressureSourceForDevTools>
WebContentsPressureManagerProxy::CreateVirtualPressureSourceForDevTools(
    device::mojom::PressureSource source,
    device::mojom::VirtualPressureSourceMetadataPtr metadata) {
  if (virtual_pressure_sources_tokens_.contains(source)) {
    return nullptr;
  }
  auto virtual_pressure_source =
      base::WrapUnique(new ScopedVirtualPressureSourceForDevTools(
          source, std::move(metadata), weak_ptr_factory_.GetWeakPtr()));
  virtual_pressure_sources_tokens_[source] = virtual_pressure_source->token();
  return virtual_pressure_source;
}

std::optional<base::UnguessableToken>
WebContentsPressureManagerProxy::GetTokenFor(
    device::mojom::PressureSource source) const {
  auto it = virtual_pressure_sources_tokens_.find(source);
  if (it == virtual_pressure_sources_tokens_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void WebContentsPressureManagerProxy::EnsureDeviceServiceConnection() {
  if (pressure_manager_) {
    return;
  }

  GetDeviceService().BindPressureManager(
      pressure_manager_.BindNewPipeAndPassReceiver());
  pressure_manager_.reset_on_disconnect();
}

device::mojom::PressureManager*
WebContentsPressureManagerProxy::GetPressureManager() {
  EnsureDeviceServiceConnection();
  return pressure_manager_.get();
}

void WebContentsPressureManagerProxy::
    OnScopedVirtualPressureSourceDevToolsDeletion(
        const ScopedVirtualPressureSourceForDevTools& virtual_pressure_source) {
  auto it =
      virtual_pressure_sources_tokens_.find(virtual_pressure_source.source());
  CHECK(it != virtual_pressure_sources_tokens_.end());
  CHECK_EQ(it->second, virtual_pressure_source.token());
  virtual_pressure_sources_tokens_.erase(it);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsPressureManagerProxy);

ScopedVirtualPressureSourceForDevTools::ScopedVirtualPressureSourceForDevTools(
    device::mojom::PressureSource source,
    device::mojom::VirtualPressureSourceMetadataPtr metadata,
    base::WeakPtr<WebContentsPressureManagerProxy>
        web_contents_pressure_manager_proxy)
    : source_(source),
      token_(base::UnguessableToken::Create()),
      web_contents_pressure_manager_proxy_(
          std::move(web_contents_pressure_manager_proxy)) {
  web_contents_pressure_manager_proxy_->GetPressureManager()
      ->AddVirtualPressureSource(token_, source, std::move(metadata),
                                 base::DoNothing());
}

ScopedVirtualPressureSourceForDevTools::
    ~ScopedVirtualPressureSourceForDevTools() {
  if (web_contents_pressure_manager_proxy_) {
    web_contents_pressure_manager_proxy_->GetPressureManager()
        ->RemoveVirtualPressureSource(token_, source_, base::DoNothing());
    web_contents_pressure_manager_proxy_
        ->OnScopedVirtualPressureSourceDevToolsDeletion(*this);
  }
}

device::mojom::PressureSource ScopedVirtualPressureSourceForDevTools::source()
    const {
  return source_;
}

base::UnguessableToken ScopedVirtualPressureSourceForDevTools::token() const {
  return token_;
}

void ScopedVirtualPressureSourceForDevTools::UpdateVirtualPressureSourceState(
    device::mojom::PressureState state,
    device::mojom::PressureManager::UpdateVirtualPressureSourceStateCallback
        callback) {
  if (web_contents_pressure_manager_proxy_) {
    web_contents_pressure_manager_proxy_->GetPressureManager()
        ->UpdateVirtualPressureSourceState(token_, source_, state,
                                           std::move(callback));
  }
}

}  // namespace content
