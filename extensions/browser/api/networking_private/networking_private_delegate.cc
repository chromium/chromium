// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_delegate.h"

#include "extensions/browser/api/networking_private/networking_private_api.h"

namespace extensions {

NetworkingPrivateDelegate::UIDelegate::UIDelegate() {}

NetworkingPrivateDelegate::UIDelegate::~UIDelegate() {}

NetworkingPrivateDelegate::NetworkingPrivateDelegate() {}

NetworkingPrivateDelegate::~NetworkingPrivateDelegate() {
}

void NetworkingPrivateDelegate::AddObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED() << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::RemoveObserver(
    NetworkingPrivateDelegateObserver* observer) {
  NOTREACHED() << "Class does not use NetworkingPrivateDelegateObserver";
}

void NetworkingPrivateDelegate::StartActivate(
    const std::string& guid,
    const std::string& carrier,
    VoidCallback success_callback,
    FailureCallback failure_callback) {
  std::move(failure_callback).Run(networking_private::kErrorNotSupported);
}

}  // namespace extensions
