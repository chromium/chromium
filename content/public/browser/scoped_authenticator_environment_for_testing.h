// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SCOPED_AUTHENTICATOR_ENVIRONMENT_FOR_TESTING_H_
#define CONTENT_PUBLIC_BROWSER_SCOPED_AUTHENTICATOR_ENVIRONMENT_FOR_TESTING_H_

#include <memory>

#include "content/common/content_export.h"

namespace device {
class FidoDiscoveryFactory;
}

namespace content {

// Allows replacing the default FidoDiscoveryFactory to support injecting
// virtual authenticators. These objects cannot be nested.
class CONTENT_EXPORT ScopedAuthenticatorEnvironmentForTesting {
 public:
  explicit ScopedAuthenticatorEnvironmentForTesting(
      std::unique_ptr<device::FidoDiscoveryFactory> factory);
  ~ScopedAuthenticatorEnvironmentForTesting();

  ScopedAuthenticatorEnvironmentForTesting(
      const ScopedAuthenticatorEnvironmentForTesting&) = delete;
  ScopedAuthenticatorEnvironmentForTesting(
      ScopedAuthenticatorEnvironmentForTesting&&) = delete;
  ScopedAuthenticatorEnvironmentForTesting& operator=(
      const ScopedAuthenticatorEnvironmentForTesting&) = delete;
  ScopedAuthenticatorEnvironmentForTesting& operator=(
      const ScopedAuthenticatorEnvironmentForTesting&&) = delete;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SCOPED_AUTHENTICATOR_ENVIRONMENT_FOR_TESTING_H_
