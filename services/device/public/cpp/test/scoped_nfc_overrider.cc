// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/scoped_nfc_overrider.h"
#include <memory>

#include "base/functional/bind.h"
#include "services/device/device_service.h"
#include "services/device/public/mojom/nfc.mojom.h"

namespace device {

namespace {

// FakeNFC which can be connected to the corresponding instance in a renderer
// process.
class FakeNFC : public mojom::NFC {
 public:
  FakeNFC() = default;
  ~FakeNFC() override = default;

  void Bind(mojo::PendingReceiver<mojom::NFC> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  // device::mojom::NFC:
  void SetClient(mojo::PendingRemote<mojom::NFCClient> client) override {}
  void Push(mojom::NDEFMessagePtr message,
            mojom::NDEFWriteOptionsPtr,
            PushCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void CancelPush() override {}
  void MakeReadOnly(MakeReadOnlyCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void CancelMakeReadOnly() override {}
  void Watch(uint32_t id, WatchCallback callback) override {
    std::move(callback).Run(nullptr);
  }
  void CancelWatch(uint32_t id) override {}

 private:
  mojo::Receiver<mojom::NFC> receiver_{this};
};

}  // namespace

// FakeNFCProvider which can be connected to the corresponding instance in the
// browser process.
class ScopedNFCOverrider::FakeNFCProvider : public mojom::NFCProvider {
 public:
  FakeNFCProvider() = default;
  ~FakeNFCProvider() override = default;

  void Bind(mojo::PendingReceiver<mojom::NFCProvider> receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(base::BindOnce(
        &FakeNFCProvider::OnMojoDisconnect, base::Unretained(this)));
    is_connected_ = true;
  }

  void OnMojoDisconnect() { is_connected_ = false; }

  // device::mojom::NFCProvider:
  void GetNFCForHost(
      int32_t host_id,
      mojo::PendingReceiver<device::mojom::NFC> receiver) override {
    fake_nfc_.Bind(std::move(receiver));
  }
  void SuspendNFCOperations() override {}
  void ResumeNFCOperations() override {}

  bool is_connected() { return is_connected_; }

 private:
  bool is_connected_{false};

  FakeNFC fake_nfc_;
  mojo::Receiver<mojom::NFCProvider> receiver_{this};
};

ScopedNFCOverrider::ScopedNFCOverrider() {
  fake_nfc_provider_ = std::make_unique<FakeNFCProvider>();
  DeviceService::OverrideNFCProviderBinderForTesting(base::BindRepeating(
      &ScopedNFCOverrider::BindFakeNFCProvider, base::Unretained(this)));
}

ScopedNFCOverrider::~ScopedNFCOverrider() {
  DeviceService::OverrideNFCProviderBinderForTesting(base::NullCallback());
}

bool ScopedNFCOverrider::IsConnected() {
  return fake_nfc_provider_->is_connected();
}

void ScopedNFCOverrider::BindFakeNFCProvider(
    mojo::PendingReceiver<device::mojom::NFCProvider> receiver) {
  fake_nfc_provider_->Bind(std::move(receiver));
}

}  // namespace device
