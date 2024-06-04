// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_audio_decoder_config.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "media/mojo/mojom/video_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace media {

class AudioDecoder;
class AudioEncoder;
class CdmFactory;
class MediaLog;
class Renderer;
class VideoDecoder;

// Provides a way for MediaService to create concrete (e.g. platform specific)
// media components’ implementations. When MediaService is created, a
// MojoMediaClient must be passed in so that MediaService knows how to create
// the media components.
class MEDIA_MOJO_EXPORT MojoMediaClient {
 public:
  // Called before the host application is scheduled to quit.
  // The application message loop is still valid at this point, so all clean
  // up tasks requiring the message loop must be completed before returning.
  virtual ~MojoMediaClient();

  // Called exactly once before any other method.
  virtual void Initialize();

  virtual std::unique_ptr<AudioDecoder> CreateAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log);

  virtual std::unique_ptr<AudioEncoder> CreateAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  virtual SupportedAudioDecoderConfigs GetSupportedAudioDecoderConfigs();

  virtual std::vector<SupportedVideoDecoderConfig>
  GetSupportedVideoDecoderConfigs();

  virtual VideoDecoderType GetDecoderImplementationType();

#if BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)
  // Ensures that the video decoder supported configurations are known. When
  // they are, |cb| is called with a PendingRemote that corresponds to the same
  // connection as |oop_video_decoder| (which may be |oop_video_decoder|
  // itself). |oop_video_decoder| may be used internally to query the supported
  // configurations of an out-of-process video decoder.
  //
  // |cb| is called with |oop_video_decoder| before NotifyDecoderSupportKnown()
  // returns if the supported configurations are already known.
  //
  // |cb| is always called on the same sequence as NotifyDecoderSupportKnown().
  virtual void NotifyDecoderSupportKnown(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<
          void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb);
#endif  // BUILDFLAG(ALLOW_OOP_VIDEO_DECODER)

  virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder);

  // Returns the Renderer to be used by MojoRendererService.
  // TODO(hubbe): Find out whether we should pass in |target_color_space| here.
  // TODO(guohuideng): Merge this function into CreateCastRenderer.
  virtual std::unique_ptr<Renderer> CreateRenderer(
      mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      const std::string& audio_device_id);

#if BUILDFLAG(ENABLE_CAST_RENDERER)
  // Used on Chromecast only.
  // When CastRenderer is created to play video content, an |overlay_plane_id|
  // is needed to indicate which |overlay_factory| this CastRenderer will be
  // associtated with.
  // Chromecast also uses CreateRenderer to create "audio only" renderers.
  virtual std::unique_ptr<Renderer> CreateCastRenderer(
      mojom::FrameInterfaceFactory* frame_interfaces,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      const base::UnguessableToken& overlay_plane_id);
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if BUILDFLAG(IS_WIN)
  virtual std::unique_ptr<Renderer> CreateMediaFoundationRenderer(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojom::FrameInterfaceFactory* frame_interfaces,
      mojo::PendingRemote<mojom::MediaLog> media_log_remote,
      mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
          renderer_extension_receiver,
      mojo::PendingRemote<media::mojom::MediaFoundationRendererClientExtension>
          client_extension_remote);
#endif  // BUILDFLAG(IS_WIN)

  // Returns the CdmFactory to be used by MojoCdmService. |frame_interfaces|
  // can be used to request interfaces provided remotely by the host. It may
  // be a nullptr if the host chose not to bind the InterfacePtr.
  virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces);

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
