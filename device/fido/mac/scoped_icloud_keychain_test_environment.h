// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_SCOPED_ICLOUD_KEYCHAIN_TEST_ENVIRONMENT_H_
#define DEVICE_FIDO_MAC_SCOPED_ICLOUD_KEYCHAIN_TEST_ENVIRONMENT_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "device/fido/discoverable_credential_metadata.h"

#if !BUILDFLAG(IS_MAC)
#error "macOS-only file"
#endif

#import <os/availability.h>

namespace device {

struct CtapMakeCredentialRequest;
struct CtapGetAssertionRequest;

namespace fido::icloud_keychain {

class FakeSystemInterface;

// ScopedTestEnvironment can be instantiated to simulate the presence of iCloud
// Keychain for tests. This header is the interface between the Obj-C++ and C++
// worlds as trying to use `fake_icloud_keychain_sys.h` directly will fail to
// compile in C++ tests.
class API_AVAILABLE(macos(13.3))
    COMPONENT_EXPORT(DEVICE_FIDO) ScopedTestEnvironment {
 public:
  explicit ScopedTestEnvironment(
      std::vector<DiscoverableCredentialMetadata> creds);
  ~ScopedTestEnvironment();

  ScopedTestEnvironment(const ScopedTestEnvironment&) = delete;
  ScopedTestEnvironment(ScopedTestEnvironment&&) = delete;
  ScopedTestEnvironment& operator=(const ScopedTestEnvironment&) = delete;
  ScopedTestEnvironment& operator=(ScopedTestEnvironment&&) = delete;

  // Configure a callback that will be called during each create request.
  void SetMakeCredentialCallback(
      base::RepeatingCallback<void(const CtapMakeCredentialRequest&)> callback);

  // Configure a callback that will be called during each get request.
  void SetGetAssertionCallback(
      base::RepeatingCallback<void(const CtapGetAssertionRequest&)> callback);

 private:
  scoped_refptr<FakeSystemInterface> fake_;
};

}  // namespace fido::icloud_keychain
}  // namespace device

#endif  // DEVICE_FIDO_MAC_SCOPED_ICLOUD_KEYCHAIN_TEST_ENVIRONMENT_H_
