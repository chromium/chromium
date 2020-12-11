// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ENUMERATION_SCREEN_ENUMERATION_IMPL_H_
#define CONTENT_BROWSER_SCREEN_ENUMERATION_SCREEN_ENUMERATION_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/screen_enumeration/screen_enumeration.mojom.h"

namespace content {

class RenderFrameHost;

// A backend for the proposed interface to query the device's screen space.
class ScreenEnumerationImpl : public blink::mojom::ScreenEnumeration {
 public:
  explicit ScreenEnumerationImpl(RenderFrameHost* render_frame_host);
  ~ScreenEnumerationImpl() override;

  ScreenEnumerationImpl(const ScreenEnumerationImpl&) = delete;
  ScreenEnumerationImpl& operator=(const ScreenEnumerationImpl&) = delete;

  // Bind a pending receiver to this backend implementation.
  void Bind(mojo::PendingReceiver<blink::mojom::ScreenEnumeration> receiver);

  // blink::mojom::ScreenEnumeration:
  void GetDisplays(GetDisplaysCallback callback) override;
  void HasMultipleDisplays(HasMultipleDisplaysCallback callback) override;

 private:
  // Called with the result of the permission request in GetDisplays().
  void GetDisplaysWithPermissionStatus(
      GetDisplaysCallback callback,
      blink::mojom::PermissionStatus permission_status);

  RenderFrameHost* render_frame_host_;
  mojo::ReceiverSet<blink::mojom::ScreenEnumeration> receivers_;
  base::WeakPtrFactory<ScreenEnumerationImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ENUMERATION_SCREEN_ENUMERATION_IMPL_H_