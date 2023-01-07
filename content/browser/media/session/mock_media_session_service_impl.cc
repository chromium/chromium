// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/mock_media_session_service_impl.h"

namespace content {

MockMediaSessionClient::MockMediaSessionClient() = default;

MockMediaSessionClient::~MockMediaSessionClient() = default;

mojo::PendingRemote<blink::mojom::MediaSessionClient>
MockMediaSessionClient::CreateInterfaceRemoteAndBind() {
  return receiver_.BindNewPipeAndPassRemote();
}

MockMediaSessionServiceImpl::MockMediaSessionServiceImpl(
    content::RenderFrameHost* rfh)
    : MediaSessionServiceImpl(rfh) {
  SetClient(mock_client_.CreateInterfaceRemoteAndBind());
}

MockMediaSessionServiceImpl::~MockMediaSessionServiceImpl() = default;

}  // namespace content
