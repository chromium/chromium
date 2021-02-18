// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "media/base/overlay_info.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/media_buildflags.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
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
class CdmFactory;
class MediaLog;
class Renderer;
class VideoDecoder;

// Map of mojo VideoDecoder implementations to the vector of configs that they
// (probably) support.
using SupportedVideoDecoderConfigMap =
    base::flat_map<VideoDecoderImplementation,
                   std::vector<SupportedVideoDecoderConfig>>;

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
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  virtual SupportedVideoDecoderConfigMap GetSupportedVideoDecoderConfigs();

  virtual std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      MediaLog* media_log,
      mojom::CommandBufferIdPtr command_buffer_id,
      VideoDecoderImplementation implementation,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space);

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

  // Returns the CdmFactory to be used by MojoCdmService. |frame_interfaces| can
  // be used to request interfaces provided remotely by the host. It may be a
  // nullptr if the host chose not to bind the InterfacePtr.
  virtual std::unique_ptr<CdmFactory> CreateCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces);

 protected:
  MojoMediaClient();
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_CLIENT_H_
