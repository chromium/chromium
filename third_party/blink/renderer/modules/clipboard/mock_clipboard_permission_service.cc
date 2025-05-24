// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/mock_clipboard_permission_service.h"

#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;

MockClipboardPermissionService::MockClipboardPermissionService() = default;
MockClipboardPermissionService::~MockClipboardPermissionService() = default;

void MockClipboardPermissionService::BindRequest(
    mojo::ScopedMessagePipeHandle handle) {
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(mojo::PendingReceiver<mojom::blink::PermissionService>(
      std::move(handle)));
  receiver_.set_disconnect_handler(
      WTF::BindOnce(&MockClipboardPermissionService::OnConnectionError,
                    WTF::Unretained(this)));
}

void MockClipboardPermissionService::OnConnectionError() {
  std::ignore = receiver_.Unbind();
}

}  // namespace blink
