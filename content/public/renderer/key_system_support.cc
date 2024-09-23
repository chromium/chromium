// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_support.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "media/base/key_systems_support_registration.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

class KeySystemSupportObserverImpl
    : public media::KeySystemSupportRegistration,
      public media::mojom::KeySystemSupportObserver {
 public:
  KeySystemSupportObserverImpl(
      media::KeySystemSupportCB cb,
      mojo::PendingReceiver<media::mojom::KeySystemSupportObserver> receiver)
      : key_system_support_cb_(std::move(cb)),
        receiver_(this, std::move(receiver)) {}

  KeySystemSupportObserverImpl(const KeySystemSupportObserverImpl&) = delete;
  KeySystemSupportObserverImpl& operator=(const KeySystemSupportObserverImpl&) =
      delete;
  ~KeySystemSupportObserverImpl() override = default;

  // media::mojom::KeySystemSupportObserver
  void OnKeySystemSupportUpdated(
      const KeySystemCapabilities& key_system_capabilities) final {
    key_system_support_cb_.Run(std::move(key_system_capabilities));
  }

 private:
  media::KeySystemSupportCB key_system_support_cb_;
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
  impl->GetBrowserInterfaceBroker().GetInterface(
      key_system_support.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_remote;
  std::unique_ptr<media::KeySystemSupportRegistration>
      key_system_support_registration =
          std::make_unique<KeySystemSupportObserverImpl>(
              std::move(cb), observer_remote.InitWithNewPipeAndPassReceiver());
  key_system_support->AddObserver(std::move(observer_remote));

  return key_system_support_registration;
}

}  // namespace content
