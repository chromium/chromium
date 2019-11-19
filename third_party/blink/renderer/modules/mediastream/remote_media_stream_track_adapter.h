// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_REMOTE_MEDIA_STREAM_TRACK_ADAPTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_REMOTE_MEDIA_STREAM_TRACK_ADAPTER_H_

#include "base/callback.h"
#include "base/logging.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/media_stream_interface.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class TrackObserver;

// Base class used for mapping between webrtc and blink MediaStream tracks.
// RemoteMediaStreamImpl has a RemoteMediaStreamTrackAdapter per remote audio
// (RemoteAudioTrackAdapter) and video (RemoteVideoTrackAdapter) track.
template <typename WebRtcMediaStreamTrackType>
class MODULES_EXPORT RemoteMediaStreamTrackAdapter
    : public WTF::ThreadSafeRefCounted<
          RemoteMediaStreamTrackAdapter<WebRtcMediaStreamTrackType>> {
 public:
  RemoteMediaStreamTrackAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      WebRtcMediaStreamTrackType* webrtc_track)
      : main_thread_(main_thread),
        webrtc_track_(webrtc_track),
        id_(WebString::FromUTF8(webrtc_track->id())) {}

  const scoped_refptr<WebRtcMediaStreamTrackType>& observed_track() {
    return webrtc_track_;
  }

  WebMediaStreamTrack* web_track() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!web_track_.IsNull());
    return &web_track_;
  }

  WebString id() const { return id_; }

  bool initialized() const {
    DCHECK(main_thread_->BelongsToCurrentThread());
    return !web_track_.IsNull();
  }

  void Initialize() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(!initialized());
    std::move(web_initialize_).Run();
    DCHECK(initialized());
  }

 protected:
  friend class WTF::ThreadSafeRefCounted<
      RemoteMediaStreamTrackAdapter<WebRtcMediaStreamTrackType>>;

  virtual ~RemoteMediaStreamTrackAdapter() {
    DCHECK(main_thread_->BelongsToCurrentThread());
  }

  void InitializeWebTrack(WebMediaStreamSource::Type type) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(web_track_.IsNull());

    WebMediaStreamSource web_source;
    web_source.Initialize(id_, type, id_, true /* remote */);
    web_track_.Initialize(id_, web_source);
    DCHECK(!web_track_.IsNull());
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // This callback will be run when Initialize() is called and then freed.
  // The callback is used by derived classes to bind objects that need to be
  // instantiated and initialized on the signaling thread but then moved to
  // and used on the main thread when initializing the web object(s).
  base::OnceClosure web_initialize_;

 private:
  const scoped_refptr<WebRtcMediaStreamTrackType> webrtc_track_;
  WebMediaStreamTrack web_track_;
  // const copy of the webrtc track id that allows us to check it from both the
  // main and signaling threads without incurring a synchronous thread hop.
  const WebString id_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamTrackAdapter);
};

class MODULES_EXPORT RemoteVideoTrackAdapter
    : public RemoteMediaStreamTrackAdapter<webrtc::VideoTrackInterface> {
 public:
  // Called on the signaling thread.
  RemoteVideoTrackAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      webrtc::VideoTrackInterface* webrtc_track);

 protected:
  ~RemoteVideoTrackAdapter() override;

 private:
  void InitializeWebVideoTrack(std::unique_ptr<TrackObserver> observer,
                               bool enabled);
};

// RemoteAudioTrackAdapter is responsible for listening on state
// change notifications on a remote webrtc audio MediaStreamTracks and notify
// Blink.
class MODULES_EXPORT RemoteAudioTrackAdapter
    : public RemoteMediaStreamTrackAdapter<webrtc::AudioTrackInterface>,
      public webrtc::ObserverInterface {
 public:
  // Called on the signaling thread.
  RemoteAudioTrackAdapter(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      webrtc::AudioTrackInterface* webrtc_track);

  void Unregister();

 protected:
  ~RemoteAudioTrackAdapter() override;

 private:
  void InitializeWebAudioTrack(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread);

  // webrtc::ObserverInterface implementation.
  void OnChanged() override;

  void OnChangedOnMainThread(
      webrtc::MediaStreamTrackInterface::TrackState state);

#if DCHECK_IS_ON()
  bool unregistered_;
#endif

  webrtc::MediaStreamTrackInterface::TrackState state_;

  DISALLOW_COPY_AND_ASSIGN(RemoteAudioTrackAdapter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_REMOTE_MEDIA_STREAM_TRACK_ADAPTER_H_
