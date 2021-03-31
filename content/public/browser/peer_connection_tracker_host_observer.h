// Copyright 2021 The Chromium Authors. All rights reserved.
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
  virtual void OnPeerConnectionAdded(GlobalFrameRoutingId render_frame_host_id,
                                     int lid,
                                     base::ProcessId pid,
                                     const std::string& url,
                                     const std::string& rtc_configuration,
                                     const std::string& constraints) {}

  // This method is called when a peer connection is destroyed.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  virtual void OnPeerConnectionRemoved(
      GlobalFrameRoutingId render_frame_host_id,
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
  // |type| == "iceConnectionStateChange" && |value| == "connected":
  //   A connection was established with another peer.
  // |type| == "stop" && |value| == "":
  //   An estasblished connection with another peer was stopped.
  virtual void OnPeerConnectionUpdated(
      GlobalFrameRoutingId render_frame_host_id,
      int lid,
      const std::string& type,
      const std::string& value) {}

  // This method is called when the session ID of a peer connection is set.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |session_id| is the session ID of the peer connection.
  virtual void OnPeerConnectionSessionIdSet(
      GlobalFrameRoutingId render_frame_host_id,
      int lid,
      const std::string& session_id) {}

  // This method is called when a WebRTC event has to be logged.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |message| is the message to be logged.
  virtual void OnWebRtcEventLogWrite(GlobalFrameRoutingId render_frame_host_id,
                                     int lid,
                                     const std::string& message) {}

  // These methods are called when results from
  // PeerConnectionInterface::GetStats() (legacy or standard API) are available.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |lid| identifies a peer connection.
  // - |value| is the list of stats reports.
  virtual void OnAddStandardStats(GlobalFrameRoutingId render_frame_host_id,
                                  int lid,
                                  base::Value value) {}
  virtual void OnAddLegacyStats(GlobalFrameRoutingId render_frame_host_id,
                                int lid,
                                base::Value value) {}

  // This method is called when getUserMedia is called.
  // - |render_frame_host_id| identifies the RenderFrameHost.
  // - |pid| is the OS process ID.
  // - |origin| is the security origin of the getUserMedia call.
  // - |audio| is true if the audio stream is requested.
  // - |video| is true if the video stream is requested.
  // - |audio_constraints| is the constraints for the audio.
  // - |video_constraints| is the constraints for the video.
  virtual void OnGetUserMedia(GlobalFrameRoutingId render_frame_host_id,
                              base::ProcessId pid,
                              const std::string& origin,
                              bool audio,
                              bool video,
                              const std::string& audio_constraints,
                              const std::string& video_constraints) {}

 protected:
  PeerConnectionTrackerHostObserver();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PEER_CONNECTION_TRACKER_HOST_OBSERVER_H_
