// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_ENVIRONMENT_H_
#define CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_ENVIRONMENT_H_

#include <memory>

#include "content/common/content_export.h"

namespace device {
class FidoDiscoveryFactory;
}

namespace content {

// Allows replacing the default FidoDiscoveryFactory to support injecting
// virtual authenticators.
class CONTENT_EXPORT AuthenticatorEnvironment {
 public:
  virtual ~AuthenticatorEnvironment() = default;

  // Returns the singleton instance.
  static AuthenticatorEnvironment* GetInstance();

  // Sets a custom FidoDiscoveryFactory to be used instead of the default real
  // FidoDiscoveryFactory.
  virtual void ReplaceDefaultDiscoveryFactoryForTesting(
      std::unique_ptr<device::FidoDiscoveryFactory> factory) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUTHENTICATOR_ENVIRONMENT_H_
