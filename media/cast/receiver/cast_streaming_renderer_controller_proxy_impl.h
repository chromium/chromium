// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RECEIVER_CAST_STREAMING_RENDERER_CONTROLLER_PROXY_IMPL_H_
#define MEDIA_CAST_RECEIVER_CAST_STREAMING_RENDERER_CONTROLLER_PROXY_IMPL_H_

#include <map>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/cast/receiver/cast_streaming_renderer_controller_proxy.h"
#include "media/cast/receiver/mojom/cast_streaming_renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
namespace cast {

// This class provides an implementation of the singleton class
// CastStreamingRendererControllerProxy.
class CastStreamingRendererControllerProxyImpl
    : public CastStreamingRendererControllerProxy {
 public:
  CastStreamingRendererControllerProxyImpl();
  ~CastStreamingRendererControllerProxyImpl() override;

  // CastStreamingRendererControllerProxy overrides.
  base::RepeatingCallback<void(
      mojo::PendingAssociatedReceiver<mojom::CastStreamingRendererController>)>
  GetBinder(content::RenderFrame* frame) override;
  mojo::PendingReceiver<media::mojom::Renderer> GetReceiver(
      content::RenderFrame* frame) override;

 private:
  // This class serves the purpose of allowing both the browser and renderer
  // processes to connect media::mojom::Renderer receiver and remote endpoints
  // without concerns of creation order or race conditions becoming concerns.
  class FrameProxy : public mojom::CastStreamingRendererController {
   public:
    explicit FrameProxy(base::RepeatingCallback<void()> disconnection_handler);
    ~FrameProxy() override;

    // Binds the frame-specific receiver from the browser process.
    void BindReceiver(
        mojo::PendingAssociatedReceiver<mojom::CastStreamingRendererController>
            pending_receiver);

    // Analogous to CastStreamingRendererControllerProxy::GetReceiver().
    mojo::PendingReceiver<media::mojom::Renderer> GetReceiver();

   private:
    // mojom::CastStreamingRendererController overrides.
    //
    // Fuses |browser_process_renderer_controls| received from the browser
    // process with  |renderer_process_remote_|, so that commands sent by the
    // browser process are passed immediately to
    // |renderer_process_pending_receiver_|.
    void SetPlaybackController(mojo::PendingReceiver<media::mojom::Renderer>
                                   browser_process_renderer_controls) override;

    // This handle is bound to the |renderer_process_pending_receiver_| upon
    // class construction. When SetPlaybackController() is called, ownership of
    // this instance will be transferred away from this class.
    mojo::PendingRemote<media::mojom::Renderer> renderer_process_remote_;

    // Owned by this class from time of creation until the first call to
    // GetReceiver(), at which time its ownership is passed to the caller.
    mojo::PendingReceiver<media::mojom::Renderer>
        renderer_process_pending_receiver_;

    // Receiver passed from the browser process to the renderer process.
    mojo::AssociatedReceiver<mojom::CastStreamingRendererController>
        browser_process_receiver_;

    base::RepeatingCallback<void()> on_mojo_disconnection_;

    THREAD_CHECKER(thread_checker_);
  };

  using RenderFrameMap =
      std::map<content::RenderFrame*, std::unique_ptr<FrameProxy>>;

  // Returned by CastStreamingRendererControllerProxyImpl::GetBinder() to allow
  // for receiving browser-process sent instances of this mojo receiver
  void BindInterface(
      content::RenderFrame* frame,
      mojo::PendingAssociatedReceiver<mojom::CastStreamingRendererController>
          pending_receiver);

  // Helper to create a new entry in |per_frame_proxies_|.
  RenderFrameMap::iterator GetFrameProxy(content::RenderFrame* frame);

  void OnRenderFrameDestroyed(content::RenderFrame* frame);

  RenderFrameMap per_frame_proxies_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RECEIVER_CAST_STREAMING_RENDERER_CONTROLLER_PROXY_IMPL_H_
