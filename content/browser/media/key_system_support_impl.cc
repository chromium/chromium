// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/key_system_support_impl.h"

#include "base/logging.h"

namespace content {

namespace {

// All key systems must have either software or hardware secure capability
// supported.
bool IsValidKeySystemCapabilities(KeySystemCapabilities capabilities) {
  for (const auto& entry : capabilities) {
    auto& capability = entry.second;
    if (!capability.sw_secure_capability.has_value() &&
        !capability.hw_secure_capability.has_value()) {
      return false;
    }
  }

  return true;
}

}  // namespace

KeySystemSupportImpl::KeySystemSupportImpl(RenderFrameHost* render_frame_host)
    : DocumentUserData(render_frame_host) {}
KeySystemSupportImpl::~KeySystemSupportImpl() = default;

void KeySystemSupportImpl::SetGetKeySystemCapabilitiesUpdateCbForTesting(
    GetKeySystemCapabilitiesUpdateCB get_support_cb_for_testing) {
  get_support_cb_for_testing_ = std::move(get_support_cb_for_testing);
}

void KeySystemSupportImpl::Bind(
    mojo::PendingReceiver<media::mojom::KeySystemSupport> receiver) {
  key_system_support_receivers_.Add(this, std::move(receiver));
}

void KeySystemSupportImpl::AddObserver(
    mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer) {
  DVLOG(3) << __func__;

  auto id = observer_remotes_.Add(std::move(observer));

  // If `key_system_support_` is already available, notify the new observer
  // immediately. All observers will be notified if there are updates later.
  if (key_system_capabilities_.has_value()) {
    observer_remotes_.Get(id)->OnKeySystemSupportUpdated(
        key_system_capabilities_.value());
    return;
  }

  // Observe key system capabilities if not have done so.
  if (!is_observing_)
    ObserveKeySystemCapabilities();
}

void KeySystemSupportImpl::ObserveKeySystemCapabilities() {
  DCHECK(!is_observing_);

  is_observing_ = true;

  auto result_cb =
      base::BindRepeating(&KeySystemSupportImpl::OnKeySystemCapabilitiesUpdated,
                          weak_ptr_factory_.GetWeakPtr());

  if (get_support_cb_for_testing_) {
    get_support_cb_for_testing_.Run(std::move(result_cb));
    return;
  }

  CdmRegistryImpl::GetInstance()->ObserveKeySystemCapabilities(
      std::move(result_cb));
}

void KeySystemSupportImpl::OnKeySystemCapabilitiesUpdated(
    KeySystemCapabilities key_system_capabilities) {
  DVLOG(3) << __func__;
  DCHECK(IsValidKeySystemCapabilities(key_system_capabilities));

  if (key_system_capabilities_.has_value() &&
      key_system_capabilities_.value() == key_system_capabilities) {
    DVLOG(1) << __func__ << ": Updated with the same key system capabilities";
    return;
  }

  key_system_capabilities_ = std::move(key_system_capabilities);

  for (auto& observer : observer_remotes_)
    observer->OnKeySystemSupportUpdated(key_system_capabilities_.value());
}

DOCUMENT_USER_DATA_KEY_IMPL(KeySystemSupportImpl);

}  // namespace content
