// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_SERVICE_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom.h"

namespace content {

class RenderFrameHost;

class KeyboardLockServiceImpl final
    : public DocumentService<blink::mojom::KeyboardLockService> {
 public:
  static void CreateMojoService(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver);

  // blink::mojom::KeyboardLockService implementation.
  void RequestKeyboardLock(const std::vector<std::string>& key_codes,
                           RequestKeyboardLockCallback callback) override;
  void CancelKeyboardLock() override;
  void GetKeyboardLayoutMap(GetKeyboardLayoutMapCallback callback) override;

 private:
  KeyboardLockServiceImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::KeyboardLockService> receiver);

  // |this| can only be destroyed by DocumentService.
  ~KeyboardLockServiceImpl() override;

  RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle feature_handle_;
  base::WeakPtrFactory<KeyboardLockServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_KEYBOARD_LOCK_KEYBOARD_LOCK_SERVICE_IMPL_H_
