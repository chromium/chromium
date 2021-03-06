// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom.h"

namespace content {

class RenderFrameHost;
class RenderFrameHostImpl;

class CONTENT_EXPORT KeyboardLockServiceImpl final
    : public FrameServiceBase<blink::mojom::KeyboardLockService> {
 public:
  KeyboardLockServiceImpl(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver);

  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver);

  // blink::mojom::KeyboardLockService implementation.
  void RequestKeyboardLock(const std::vector<std::string>& key_codes,
                           RequestKeyboardLockCallback callback) override;
  void CancelKeyboardLock() override;
  void GetKeyboardLayoutMap(GetKeyboardLayoutMapCallback callback) override;

 private:
  // |this| can only be destroyed by FrameServiceBase.
  ~KeyboardLockServiceImpl() override;

  RenderFrameHostImpl* const render_frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_SERVICE_IMPL_H_
