// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a host that receives commands from a Chromoting client.
//
// This interface handles control messages defined in control.proto.

#ifndef REMOTING_PROTOCOL_HOST_STUB_H_
#define REMOTING_PROTOCOL_HOST_STUB_H_

namespace remoting::protocol {

class AudioControl;
class Capabilities;
class ClientResolution;
class ExtensionMessage;
class PairingRequest;
class PeerConnectionParameters;
class SelectDesktopDisplayRequest;
class VideoControl;
class VideoLayout;

class HostStub {
 public:
  HostStub() = default;

  HostStub(const HostStub&) = delete;
  HostStub& operator=(const HostStub&) = delete;

  // Notification of the client dimensions and pixel density.
  // This may be used to resize the host display to match the client area.
  virtual void NotifyClientResolution(const ClientResolution& resolution) = 0;

  // Configures video update properties. Currently only pausing & resuming the
  // video channel is supported.
  virtual void ControlVideo(const VideoControl& video_control) = 0;

  // Configures audio properties. Currently only pausing & resuming the audio
  // channel is supported.
  virtual void ControlAudio(const AudioControl& audio_control) = 0;

  // Configures peer connection. This will have no effect if the host doesn't
  // support the parameters or the parameters are invalid.
  virtual void ControlPeerConnection(
      const PeerConnectionParameters& parameters) = 0;

  // Passes the set of capabilities supported by the client to the host.
  virtual void SetCapabilities(const Capabilities& capabilities) = 0;

  // Requests pairing between the host and client for PIN-less authentication.
  virtual void RequestPairing(const PairingRequest& pairing_request) = 0;

  // Deliver an extension message from the client to the host.
  virtual void DeliverClientMessage(const ExtensionMessage& message) = 0;

  // Select the specified host display.
  virtual void SelectDesktopDisplay(
      const SelectDesktopDisplayRequest& select_display) = 0;

  // Changes the current video layout.
  virtual void SetVideoLayout(const VideoLayout& video_layout) = 0;

 protected:
  virtual ~HostStub() = default;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_HOST_STUB_H_
