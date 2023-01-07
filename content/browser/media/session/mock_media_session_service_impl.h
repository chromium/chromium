// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MOCK_MEDIA_SESSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MOCK_MEDIA_SESSION_SERVICE_IMPL_H_

#include "content/browser/media/session/media_session_service_impl.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

namespace content {

class MockMediaSessionClient : public blink::mojom::MediaSessionClient {
 public:
  MockMediaSessionClient();

  MockMediaSessionClient(const MockMediaSessionClient&) = delete;
  MockMediaSessionClient& operator=(const MockMediaSessionClient&) = delete;

  ~MockMediaSessionClient() override;

  mojo::PendingRemote<blink::mojom::MediaSessionClient>
  CreateInterfaceRemoteAndBind();

  MOCK_METHOD2(DidReceiveAction,
               void(media_session::mojom::MediaSessionAction action,
                    blink::mojom::MediaSessionActionDetailsPtr details));

 private:
  mojo::Receiver<blink::mojom::MediaSessionClient> receiver_{this};
};

class MockMediaSessionServiceImpl : public content::MediaSessionServiceImpl {
 public:
  explicit MockMediaSessionServiceImpl(content::RenderFrameHost* rfh);
  ~MockMediaSessionServiceImpl() override;

  MockMediaSessionClient& mock_client() { return mock_client_; }

 private:
  MockMediaSessionClient mock_client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MOCK_MEDIA_SESSION_SERVICE_IMPL_H_
