// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_COOKIE_ENCRYPTION_PROVIDER_IMPL_H_
#define SERVICES_NETWORK_PUBLIC_CPP_COOKIE_ENCRYPTION_PROVIDER_IMPL_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/cookie_encryption_provider.mojom.h"

namespace os_crypt_async {
class OSCryptAsync;
}

// Implementation of CookieEncryptionProvider interface. This is Windows only
// for now, but will be expanded to other platforms in future.
class COMPONENT_EXPORT(NETWORK_CPP) CookieEncryptionProviderImpl
    : public network::mojom::CookieEncryptionProvider {
 public:
  explicit CookieEncryptionProviderImpl(
      os_crypt_async::OSCryptAsync* os_crypt_async);
  ~CookieEncryptionProviderImpl() override;

  CookieEncryptionProviderImpl(const CookieEncryptionProviderImpl&) = delete;
  CookieEncryptionProviderImpl& operator=(const CookieEncryptionProviderImpl&) =
      delete;

  // mojom::CookieEncryptionProvider implementation.
  void GetEncryptor(GetEncryptorCallback callback) override;

  // Returns a mojo::PendingRemote to this instance. Adds a receiver to
  // `receivers_`.
  mojo::PendingRemote<network::mojom::CookieEncryptionProvider> BindNewRemote();

 private:
  mojo::ReceiverSet<network::mojom::CookieEncryptionProvider> receivers_;
  raw_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

#endif  // SERVICES_NETWORK_PUBLIC_CPP_COOKIE_ENCRYPTION_PROVIDER_IMPL_H_
