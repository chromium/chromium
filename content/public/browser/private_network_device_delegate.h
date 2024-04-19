// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRIVATE_NETWORK_DEVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_PRIVATE_NETWORK_DEVICE_DELEGATE_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"

namespace content {

class RenderFrameHost;

// Interface provided by the content embedder to support the private network
// access user permission.
class CONTENT_EXPORT PrivateNetworkDeviceDelegate {
 public:
  virtual ~PrivateNetworkDeviceDelegate() = default;

  // Request permission for the Private Network Device.
  // |callback| accepts a bool specifying whether the user granted permission.
  //
  // TODO(crbug.com/40272624): Check if there's permission in the storage
  // already. If not, show the chooser.
  virtual void RequestPermission(RenderFrameHost& frame,
                                 blink::mojom::PrivateNetworkDevicePtr device,
                                 base::OnceCallback<void(bool)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRIVATE_NETWORK_DEVICE_DELEGATE_H_
