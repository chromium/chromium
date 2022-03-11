// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace content {

// static
KeySystemSupportImpl* KeySystemSupportImpl::GetInstance() {
  static base::NoDestructor<KeySystemSupportImpl> impl;
  return impl.get();
}

// static
void KeySystemSupportImpl::BindReceiver(
    mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver) {
  KeySystemSupportImpl::GetInstance()->Bind(std::move(receiver));
}

KeySystemSupportImpl::KeySystemSupportImpl(
    GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing) {
  auto result_cb =
      base::BindRepeating(&KeySystemSupportImpl::OnKeySystemCapabilitiesUpdated,
                          weak_ptr_factory_.GetWeakPtr());

  if (get_support_cb_for_testing) {
    get_support_cb_for_testing.Run(std::move(result_cb));
    return;
  }

  CdmRegistryImpl::GetInstance()->ObserveKeySystemCapabilities(
      std::move(result_cb));
}

KeySystemSupportImpl::~KeySystemSupportImpl() = default;

void KeySystemSupportImpl::Bind(
    mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver) {
  key_system_support_receivers_.Add(this, std::move(receiver));
}

void KeySystemSupportImpl::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(3) << __func__ << ": key_system=" << key_system;

  if (!key_system_capabilities_.has_value()) {
    pending_callbacks_.emplace_back(key_system, std::move(callback));
    return;
  }

  DCHECK(pending_callbacks_.empty());

  NotifyIsKeySystemSupportedCallback(key_system, std::move(callback));
}

void KeySystemSupportImpl::OnKeySystemCapabilitiesUpdated(
    KeySystemCapabilities key_system_capabilities) {
  DVLOG(3) << __func__;
  key_system_capabilities_ = std::move(key_system_capabilities);

  PendingCallbacks callbacks;
  pending_callbacks_.swap(callbacks);

  for (auto& entry : callbacks) {
    auto& key_system = entry.first;
    auto& callback = entry.second;
    NotifyIsKeySystemSupportedCallback(key_system, std::move(callback));
  }
}

void KeySystemSupportImpl::NotifyIsKeySystemSupportedCallback(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(3) << __func__ << ": key_system=" << key_system;
  DCHECK(key_system_capabilities_.has_value());

  if (!key_system_capabilities_->count(key_system)) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  auto key_system_capabilities =
      key_system_capabilities_.value()[key_system].Clone();
  DCHECK(key_system_capabilities);

  if (!key_system_capabilities->sw_secure_capability.has_value() &&
      !key_system_capabilities->hw_secure_capability.has_value()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  std::move(callback).Run(true, std::move(key_system_capabilities));
}

}  // namespace content
