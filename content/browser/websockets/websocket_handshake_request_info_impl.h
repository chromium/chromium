// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBSOCKET_WEBSOCKET_HANDSHAKE_REQUEST_INFO_IMPL_H_
#define CONTENT_BROWSER_WEBSOCKET_WEBSOCKET_HANDSHAKE_REQUEST_INFO_IMPL_H_

#include "content/public/browser/websocket_handshake_request_info.h"

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace content {

class WebSocketHandshakeRequestInfoImpl final
    : public WebSocketHandshakeRequestInfo,
      public base::SupportsUserData::Data {
 public:
  ~WebSocketHandshakeRequestInfoImpl() override;

  static void CreateInfoAndAssociateWithRequest(int child_id,
                                                int render_frame_id,
                                                net::URLRequest* request);

  int GetChildId() override;
  int GetRenderFrameId() override;

 private:
  WebSocketHandshakeRequestInfoImpl(int child_id, int render_frame_id);

  const int child_id_;
  const int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketHandshakeRequestInfoImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBSOCKET_WEBSOCKET_HANDSHAKE_REQUEST_INFO_IMPL_H_
