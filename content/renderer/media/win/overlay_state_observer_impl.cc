// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/win/overlay_state_observer_impl.h"

#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

// static
std::unique_ptr<media::OverlayStateObserverSubscription>
OverlayStateObserverImpl::Create(
    OverlayStateServiceProvider* overlay_state_service_provider,
    const gpu::Mailbox& mailbox,
    StateChangedCB state_changed_cb) {
  if (overlay_state_service_provider) {
    return base::WrapUnique(new OverlayStateObserverImpl(
        overlay_state_service_provider, mailbox, state_changed_cb));
  }
  return nullptr;
}

OverlayStateObserverImpl::OverlayStateObserverImpl(
    OverlayStateServiceProvider* overlay_state_service_provider,
    const gpu::Mailbox& mailbox,
    StateChangedCB state_changed_cb)
    : callback_(std::move(state_changed_cb)), receiver_(this) {
  bool succeeded = overlay_state_service_provider->RegisterObserver(
      receiver_.BindNewPipeAndPassRemote(), mailbox);
  DCHECK(succeeded);
}

OverlayStateObserverImpl::~OverlayStateObserverImpl() = default;

void OverlayStateObserverImpl::OnStateChanged(bool promoted) {
  callback_.Run(promoted);
}

}  // namespace content
