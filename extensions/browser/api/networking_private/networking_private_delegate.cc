// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_delegate.h"

#include "extensions/browser/api/networking_private/networking_private_api.h"

namespace extensions {

NetworkingPrivateDelegate::UIDelegate::UIDelegate() = default;

NetworkingPrivateDelegate::UIDelegate::~UIDelegate() = default;

NetworkingPrivateDelegate::NetworkingPrivateDelegate() = default;

NetworkingPrivateDelegate::~NetworkingPrivateDelegate() = default;

void NetworkingPrivateDelegate::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED_IN_MIGRATION()
      << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED_IN_MIGRATION()
      << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::StartActivate(
    const std::string& guid,
    const std::string& carrier,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

}  // namespace extensions
