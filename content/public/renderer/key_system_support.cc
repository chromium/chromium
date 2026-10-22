// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_support.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "media/base/cdm_capability.h"
#include "media/base/key_systems_support_registration.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/platform/task_type.h"

namespace content {

namespace {

// Replaces a pending status with an error, which reports the capability as
// unsupported.
void ResolveIfPending(media::CdmCapabilityOrStatus& capability_or_status) {
  if (capability_or_status ==
      base::unexpected(media::CdmCapabilityQueryStatus::kPending)) {
    capability_or_status =
        base::unexpected(media::CdmCapabilityQueryStatus::kDisconnectionError);
  }
}

class KeySystemSupportObserverImpl
    : public media::KeySystemSupportRegistration,
      public media::mojom::KeySystemSupportObserver {
 public:
  KeySystemSupportObserverImpl(
      media::KeySystemSupportCB cb,
      mojo::PendingReceiver<media::mojom::KeySystemSupportObserver> receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : key_system_support_cb_(std::move(cb)),
        receiver_(this, std::move(receiver), std::move(task_runner)) {
    // base::Unretained is safe because `this` owns `receiver_`.
    receiver_.set_disconnect_handler(base::BindOnce(
        &KeySystemSupportObserverImpl::OnDisconnect, base::Unretained(this)));
  }

  KeySystemSupportObserverImpl(const KeySystemSupportObserverImpl&) = delete;
  KeySystemSupportObserverImpl& operator=(const KeySystemSupportObserverImpl&) =
      delete;
  ~KeySystemSupportObserverImpl() override = default;

  // media::mojom::KeySystemSupportObserver
  void OnKeySystemSupportUpdated(
      const KeySystemCapabilities& key_system_capabilities) final {
    key_system_capabilities_ = key_system_capabilities;
    key_system_support_cb_.Run(key_system_capabilities);
  }

 private:
  // No more updates arrive after a disconnect, e.g. after the frame navigates
  // away from its initial empty document. Report pending capabilities as
  // unsupported so that queries waiting for them don't hang.
  void OnDisconnect() {
    for (auto& [key_system, capability] : key_system_capabilities_) {
      ResolveIfPending(capability.sw_cdm_capability_or_status);
      ResolveIfPending(capability.hw_cdm_capability_or_status);
    }
    key_system_support_cb_.Run(key_system_capabilities_);
  }

  media::KeySystemSupportCB key_system_support_cb_;
  // The last capabilities received from the browser.
  KeySystemCapabilities key_system_capabilities_;
  mojo::Receiver<media::mojom::KeySystemSupportObserver> receiver_;
};

}  // namespace

std::unique_ptr<media::KeySystemSupportRegistration>
ObserveKeySystemSupportUpdate(content::RenderFrame* render_frame,
                              media::KeySystemSupportCB cb) {
  DVLOG(1) << __func__;

  // `key_system_support` will stay alive as long as the returned value of this
  // function is not destructed by the caller.
  mojo::Remote<media::mojom::KeySystemSupport> key_system_support;
  RenderFrameImpl* impl = static_cast<RenderFrameImpl*>(render_frame);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      impl->GetTaskRunner(blink::TaskType::kInternalMedia);
  impl->GetBrowserInterfaceBroker().GetInterface(
      key_system_support.BindNewPipeAndPassReceiver(task_runner));

  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_remote;
  std::unique_ptr<media::KeySystemSupportRegistration>
      key_system_support_registration =
          std::make_unique<KeySystemSupportObserverImpl>(
              std::move(cb), observer_remote.InitWithNewPipeAndPassReceiver(),
              std::move(task_runner));
  key_system_support->SetObserver(std::move(observer_remote));

  return key_system_support_registration;
}

}  // namespace content
