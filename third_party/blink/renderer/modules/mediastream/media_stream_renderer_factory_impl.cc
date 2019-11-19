// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_renderer_factory_impl.h"

#include <utility>

#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/modules/webrtc/webrtc_logging.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/public/web/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/public/web/modules/webrtc/webrtc_audio_device_impl.h"
#include "third_party/blink/public/web/modules/webrtc/webrtc_audio_renderer.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_renderer_sink.h"
#include "third_party/blink/renderer/modules/mediastream/track_audio_renderer.h"
#include "third_party/blink/renderer/platform/webrtc/peer_connection_remote_audio_source.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace blink {

namespace {

// Returns a valid session id if a single WebRTC capture device is currently
// open (and then the matching session_id), otherwise 0.
// This is used to pass on a session id to an audio renderer, so that audio will
// be rendered to a matching output device, should one exist.
// Note that if there are more than one open capture devices the function
// will not be able to pick an appropriate device and return 0.
base::UnguessableToken GetSessionIdForWebRtcAudioRenderer() {
  WebRtcAudioDeviceImpl* audio_device =
      PeerConnectionDependencyFactory::GetInstance()->GetWebRtcAudioDevice();
  return audio_device
             ? audio_device->GetAuthorizedDeviceSessionIdForAudioRenderer()
             : base::UnguessableToken();
}

}  // namespace

std::unique_ptr<WebMediaStreamRendererFactory>
CreateWebMediaStreamRendererFactory() {
  return std::make_unique<MediaStreamRendererFactoryImpl>();
}

MediaStreamRendererFactoryImpl::MediaStreamRendererFactoryImpl() {}

MediaStreamRendererFactoryImpl::~MediaStreamRendererFactoryImpl() {}

scoped_refptr<WebMediaStreamVideoRenderer>
MediaStreamRendererFactoryImpl::GetVideoRenderer(
    const WebMediaStream& web_stream,
    const WebMediaStreamVideoRenderer::RepaintCB& repaint_cb,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner) {
  DCHECK(!web_stream.IsNull());

  DVLOG(1) << "MediaStreamRendererFactoryImpl::GetVideoRenderer stream:"
           << web_stream.Id().Utf8();

  WebVector<WebMediaStreamTrack> video_tracks = web_stream.VideoTracks();
  if (video_tracks.empty() ||
      !MediaStreamVideoTrack::GetTrack(video_tracks[0])) {
    return nullptr;
  }

  return new MediaStreamVideoRendererSink(video_tracks[0], repaint_cb,
                                          std::move(io_task_runner),
                                          std::move(main_render_task_runner));
}

scoped_refptr<WebMediaStreamAudioRenderer>
MediaStreamRendererFactoryImpl::GetAudioRenderer(
    const WebMediaStream& web_stream,
    WebLocalFrame* web_frame,
    const WebString& device_id) {
  DCHECK(!web_stream.IsNull());
  WebVector<WebMediaStreamTrack> audio_tracks = web_stream.AudioTracks();
  if (audio_tracks.empty()) {
    WebRtcLogMessage("No audio tracks in media stream (return null).");
    return nullptr;
  }

  DVLOG(1) << "MediaStreamRendererFactoryImpl::GetAudioRenderer stream:"
           << web_stream.Id().Utf8();

  // TODO(tommi): We need to fix the data flow so that
  // it works the same way for all track implementations, local, remote or what
  // have you.
  // In this function, we should simply create a renderer object that receives
  // and mixes audio from all the tracks that belong to the media stream.
  // For now, we have separate renderers depending on if the first audio track
  // in the stream is local or remote.
  MediaStreamAudioTrack* audio_track =
      MediaStreamAudioTrack::From(audio_tracks[0]);
  if (!audio_track) {
    // This can happen if the track was cloned.
    // TODO(tommi, perkj): Fix cloning of tracks to handle extra data too.
    WebRtcLogMessage("Error: No native track for WebMediaStreamTrack");
    return nullptr;
  }

  // If the track has a local source, or is a remote track that does not use the
  // WebRTC audio pipeline, return a new TrackAudioRenderer instance.
  if (!PeerConnectionRemoteAudioTrack::From(audio_track)) {
    // TODO(xians): Add support for the case where the media stream contains
    // multiple audio tracks.
    DVLOG(1) << "Creating TrackAudioRenderer for "
             << (audio_track->is_local_track() ? "local" : "remote")
             << " track.";
    return new TrackAudioRenderer(audio_tracks[0], web_frame,
                                  /*session_id=*/base::UnguessableToken(),
                                  String(device_id));
  }

  // This is a remote WebRTC media stream.
  WebRtcAudioDeviceImpl* audio_device =
      PeerConnectionDependencyFactory::GetInstance()->GetWebRtcAudioDevice();
  DCHECK(audio_device);

  // Share the existing renderer if any, otherwise create a new one.
  scoped_refptr<WebRtcAudioRenderer> renderer(audio_device->renderer());
  if (renderer) {
    DVLOG(1) << "Using existing WebRtcAudioRenderer for remote WebRTC track.";
  } else {
    DVLOG(1) << "Creating WebRtcAudioRenderer for remote WebRTC track.";

    renderer = new WebRtcAudioRenderer(
        PeerConnectionDependencyFactory::GetInstance()
            ->GetWebRtcSignalingTaskRunner(),
        web_stream, web_frame, GetSessionIdForWebRtcAudioRenderer(),
        device_id.Utf8());

    if (!audio_device->SetAudioRenderer(renderer.get())) {
      WebRtcLogMessage("Error: SetAudioRenderer failed for remote track.");
      return nullptr;
    }
  }

  auto ret = renderer->CreateSharedAudioRendererProxy(web_stream);
  if (!ret)
    WebRtcLogMessage("Error: CreateSharedAudioRendererProxy failed.");
  return ret;
}

}  // namespace blink
