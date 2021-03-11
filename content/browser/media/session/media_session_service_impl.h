// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/mediasession/media_session.mojom.h"

namespace content {

class RenderFrameHost;
class MediaSessionImpl;

// There is one MediaSessionService per frame. The class is owned by
// RenderFrameHost and should register/unregister itself to/from
// MediaSessionImpl when RenderFrameHost is created/destroyed.
class CONTENT_EXPORT MediaSessionServiceImpl
    : public blink::mojom::MediaSessionService {
 public:
  ~MediaSessionServiceImpl() override;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::MediaSessionService> receiver);
  const mojo::Remote<blink::mojom::MediaSessionClient>& GetClient() {
    return client_;
  }
  RenderFrameHost* GetRenderFrameHost();

  blink::mojom::MediaSessionPlaybackState playback_state() const {
    return playback_state_;
  }
  const blink::mojom::SpecMediaMetadataPtr& metadata() const {
    return metadata_;
  }
  const std::set<media_session::mojom::MediaSessionAction>& actions() const {
    return actions_;
  }
  const base::Optional<media_session::MediaPosition>& position() const {
    return position_;
  }

  void DidFinishNavigation();
  void FlushForTesting();

  // blink::mojom::MediaSessionService implementation.
  void SetClient(
      mojo::PendingRemote<blink::mojom::MediaSessionClient> client) override;

  void SetPlaybackState(blink::mojom::MediaSessionPlaybackState state) override;
  void SetPositionState(
      const base::Optional<media_session::MediaPosition>& position) override;
  void SetMetadata(blink::mojom::SpecMediaMetadataPtr metadata) override;

  void EnableAction(media_session::mojom::MediaSessionAction action) override;
  void DisableAction(media_session::mojom::MediaSessionAction action) override;

 protected:
  explicit MediaSessionServiceImpl(RenderFrameHost* render_frame_host);

 private:
  MediaSessionImpl* GetMediaSession();

  void Bind(mojo::PendingReceiver<blink::mojom::MediaSessionService> receiver);

  void ClearActions();

  const int render_frame_process_id_;
  const int render_frame_routing_id_;

  // RAII binding of |this| to an MediaSessionService interface request.
  // The binding is removed when receiver_ is cleared or goes out of scope.
  std::unique_ptr<mojo::Receiver<blink::mojom::MediaSessionService>> receiver_;
  mojo::Remote<blink::mojom::MediaSessionClient> client_;
  blink::mojom::MediaSessionPlaybackState playback_state_;
  blink::mojom::SpecMediaMetadataPtr metadata_;
  std::set<media_session::mojom::MediaSessionAction> actions_;
  base::Optional<media_session::MediaPosition> position_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_SERVICE_IMPL_H_
