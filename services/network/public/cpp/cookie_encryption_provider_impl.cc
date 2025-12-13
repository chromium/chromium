// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cookie_encryption_provider_impl.h"

#include "components/os_crypt/async/browser/os_crypt_async.h"

CookieEncryptionProviderImpl::CookieEncryptionProviderImpl(
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : os_crypt_async_(os_crypt_async) {}

CookieEncryptionProviderImpl::~CookieEncryptionProviderImpl() = default;

void CookieEncryptionProviderImpl::GetEncryptor(GetEncryptorCallback callback) {
  os_crypt_async_->GetInstance(base::BindOnce(
      [](GetEncryptorCallback callback, os_crypt_async::Encryptor encryptor) {
        std::move(callback).Run(std::move(encryptor));
      },
      std::move(callback)));
}

mojo::PendingRemote<network::mojom::CookieEncryptionProvider>
CookieEncryptionProviderImpl::BindNewRemote() {
  mojo::PendingRemote<network::mojom::CookieEncryptionProvider> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}
