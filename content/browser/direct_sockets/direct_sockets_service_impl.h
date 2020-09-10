// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace content {

// Implementation of the DirectSocketsService Mojo service.
class CONTENT_EXPORT DirectSocketsServiceImpl
    : public blink::mojom::DirectSocketsService,
      public WebContentsObserver {
 public:
  using PermissionCallback = base::RepeatingCallback<net::Error(
      const blink::mojom::DirectSocketOptions&)>;

  explicit DirectSocketsServiceImpl(RenderFrameHost& frame_host);
  ~DirectSocketsServiceImpl() override = default;

  DirectSocketsServiceImpl(const DirectSocketsServiceImpl&) = delete;
  DirectSocketsServiceImpl& operator=(const DirectSocketsServiceImpl&) = delete;

  static void CreateForFrame(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver);

  // blink::mojom::DirectSocketsService override:
  void OpenTcpSocket(blink::mojom::DirectSocketOptionsPtr options,
                     OpenTcpSocketCallback callback) override;
  void OpenUdpSocket(blink::mojom::DirectSocketOptionsPtr options,
                     OpenUdpSocketCallback callback) override;

  // WebContentsObserver override:
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  static void SetPermissionCallbackForTesting(PermissionCallback callback);

 private:
  friend class DirectSocketsUnitTest;

  net::Error EnsurePermission(const blink::mojom::DirectSocketOptions& options);

  network::mojom::NetworkContext* GetNetworkContext();

  RenderFrameHost* frame_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIRECT_SOCKETS_DIRECT_SOCKETS_SERVICE_IMPL_H_
