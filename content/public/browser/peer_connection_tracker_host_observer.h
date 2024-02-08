// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PEER_CONNECTION_TRACKER_HOST_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_PEER_CONNECTION_TRACKER_HOST_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "base/process/process_handle.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

// An observer that gets notified with events related to WebRTC peer
// connections. Must be used on the UI thread only.
class CONTENT_EXPORT PeerConnectionTrackerHostObserver
    : public base::CheckedObserver {
 public:
  ~PeerConnectionTrackerHostObserver() override;

  // This method is called when a peer connection is created.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |pid| is the OS process ID.
  // - |url| is the URL of the tab owning the peer connection.
  // - |rtc_configuration| is the serialized RTCConfiguration.
  // - |constraints| is the media constraints used to initialize the peer
  //   connection.
  virtual void OnPeerConnectionAdded(
      GlobalRenderFrameHostId render_frame_host_id,
      int lid,
      base::ProcessId pid,
      const std::string& url,
      const std::string& rtc_configuration) {}

  // This method is called when a peer connection is destroyed.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  virtual void OnPeerConnectionRemoved(
      GlobalRenderFrameHostId render_frame_host_id,
      int lid) {}

  // This method is called when a peer connection is updated.
  //
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |type| is the update type.
  // - |value| is the detail of the update.
  //
  // There are many possible values for |type| and |value|. Here are a couple
  // examples:
  // |type| == "connectionstatechange" && |value| == "connected":
  //   A connection was established with another peer.
  // |type| == "close" && |value| == "":
  //   An established connection with another peer was closed.
  virtual void OnPeerConnectionUpdated(
      GlobalRenderFrameHostId render_frame_host_id,
      int lid,
      const std::string& type,
      const std::string& value) {}

  // This method is called when the session ID of a peer connection is set.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |session_id| is the session ID of the peer connection.
  virtual void OnPeerConnectionSessionIdSet(
      GlobalRenderFrameHostId render_frame_host_id,
      int lid,
      const std::string& session_id) {}

  // This method is called when a WebRTC event has to be logged.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |message| is the message to be logged.
  virtual void OnWebRtcEventLogWrite(
      GlobalRenderFrameHostId render_frame_host_id,
      int lid,
      const std::string& message) {}

  // These methods are called when results from
  // PeerConnectionInterface::GetStats() (legacy or standard API) are available.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |value| is the list of stats reports.
  virtual void OnAddStandardStats(GlobalRenderFrameHostId render_frame_host_id,
                                  int lid,
                                  base::Value::List value) {}
  virtual void OnAddLegacyStats(GlobalRenderFrameHostId render_frame_host_id,
                                int lid,
                                base::Value::List value) {}

  // This method is called when getUserMedia is called.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |request_id| is an id assigned to the getUserMedia call and its
  //     callback/error
  // - |audio| is true if the audio stream is requested.
  // - |video| is true if the video stream is requested.
  // - |audio_constraints| is the constraints for the audio.
  // - |video_constraints| is the constraints for the video.
  virtual void OnGetUserMedia(GlobalRenderFrameHostId render_frame_host_id,
                              base::ProcessId pid,
                              int request_id,
                              bool audio,
                              bool video,
                              const std::string& audio_constraints,
                              const std::string& video_constraints) {}

  // This method is called when getUserMedia resolves with a stream.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |request_id| is the internal getUserMedia request id.
  // - |stream_id| is the id of the stream containing the tracks.
  // - |audio_track_info| describes the streams audio track (if any).
  // - |video_track_info| describes the streams video track (if any).
  virtual void OnGetUserMediaSuccess(
      GlobalRenderFrameHostId render_frame_host_id,
      base::ProcessId pid,
      int request_id,
      const std::string& stream_id,
      const std::string& audio_track_info,
      const std::string& video_track_info) {}

  // This method is called when getUserMedia rejects with an error.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |request_id| is the internal getUserMedia request id.
  // - |error| is the (DOM) error.
  // - |error_message| is the error message.
  virtual void OnGetUserMediaFailure(
      GlobalRenderFrameHostId render_frame_host_id,
      base::ProcessId pid,
      int request_id,
      const std::string& error,
      const std::string& error_message) {}

  // This method is called when getDisplayMedia is called.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |request_id| is an id assigned to the getDisplayMedia call and its
  //     callback/error
  // - |audio| is true if the audio stream is requested.
  // - |video| is true if the video stream is requested.
  // - |audio_constraints| is the constraints for the audio.
  // - |video_constraints| is the constraints for the video.
  virtual void OnGetDisplayMedia(GlobalRenderFrameHostId render_frame_host_id,
                                 base::ProcessId pid,
                                 int request_id,
                                 bool audio,
                                 bool video,
                                 const std::string& audio_constraints,
                                 const std::string& video_constraints) {}

  // This method is called when getDisplayMedia resolves with a stream.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |request_id| is the internal getDisplayMedia request id.
  // - |stream_id| is the id of the stream containing the tracks.
  // - |audio_track_info| describes the streams audio track (if any).
  // - |video_track_info| describes the streams video track (if any).
  virtual void OnGetDisplayMediaSuccess(
      GlobalRenderFrameHostId render_frame_host_id,
      base::ProcessId pid,
      int request_id,
      const std::string& stream_id,
      const std::string& audio_track_info,
      const std::string& video_track_info) {}

  // This method is called when getDisplayMedia rejects with an error.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |request_id| is the internal getDisplayMedia request id.
  // - |error| is the (DOM) error.
  // - |error_message| is the error message.
  virtual void OnGetDisplayMediaFailure(
      GlobalRenderFrameHostId render_frame_host_id,
      base::ProcessId pid,
      int request_id,
      const std::string& error,
      const std::string& error_message) {}

 protected:
  PeerConnectionTrackerHostObserver();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEER_CONNECTION_TRACKER_HOST_OBSERVER_H_
