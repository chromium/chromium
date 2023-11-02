// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/key_system_support.h"

#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

class KeySystemSupportObserverImpl
    : public media::mojom::KeySystemSupportObserver {
 public:
  explicit KeySystemSupportObserverImpl(KeySystemSupportCB cb)
      : key_system_support_cb_(std::move(cb)) {}
  KeySystemSupportObserverImpl(const KeySystemSupportObserverImpl&) = delete;
  KeySystemSupportObserverImpl& operator=(const KeySystemSupportObserverImpl&) =
      delete;
  ~KeySystemSupportObserverImpl() override = default;

  // media::mojom::KeySystemSupportObserver
  void OnKeySystemSupportUpdated(
      KeySystemCapabilityPtrMap key_system_capabilities) final {
    key_system_support_cb_.Run(std::move(key_system_capabilities));
  }

 private:
  KeySystemSupportCB key_system_support_cb_;
};

}  // namespace

void ObserveKeySystemSupportUpdate(KeySystemSupportCB cb) {
  DVLOG(1) << __func__;

  // `key_system_support` will be destructed after this function returns. This
  // is fine since the observer will stay registered in KeySystemSupportImpl,
  // which is a singleton in the browser process.
  mojo::Remote<media::mojom::KeySystemSupport> key_system_support;
  content::RenderThread::Get()->BindHostReceiver(
      key_system_support.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<media::mojom::KeySystemSupportObserver> observer_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<KeySystemSupportObserverImpl>(std::move(cb)),
      observer_remote.InitWithNewPipeAndPassReceiver());
  key_system_support->AddObserver(std::move(observer_remote));
}

}  // namespace content
