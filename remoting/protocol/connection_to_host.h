// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
#define REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/errors.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {
namespace protocol {

class AudioStub;
class ClientStub;
class ClipboardStub;
class HostStub;
class InputStub;
class Session;
class SessionConfig;
class TransportContext;
struct TransportRoute;
class VideoRenderer;

class ConnectionToHost {
 public:
  // The UI implementations maintain corresponding definitions of this
  // enumeration in client_session.js and
  // android/java/src/org/chromium/chromoting/jni/JniInterface.java. Be sure to
  // update these locations if you make any changes to the ordering.
  enum State {
    INITIALIZING,
    CONNECTING,
    AUTHENTICATED,
    CONNECTED,
    FAILED,
    CLOSED,
  };

  // Returns the literal string of |state|.
  static const char* StateToString(State state);

  class HostEventCallback {
   public:
    virtual ~HostEventCallback() {}

    // Called when state of the connection changes.
    virtual void OnConnectionState(State state, ErrorCode error) = 0;

    // Called when ready state of the connection changes. When |ready|
    // is set to false some data sent by the peers may be
    // delayed. This is used to indicate in the UI when connection is
    // temporarily broken.
    virtual void OnConnectionReady(bool ready) = 0;

    // Called when the route type (direct vs. STUN vs. proxied) changes.
    virtual void OnRouteChanged(const std::string& channel_name,
                                const protocol::TransportRoute& route) = 0;
  };

  virtual ~ConnectionToHost() {}

  // Set the stubs which will handle messages from the host.
  // The caller must ensure that stubs out-live the connection.
  // Unless otherwise specified, all stubs must be set before Connect()
  // is called.
  virtual void set_client_stub(ClientStub* client_stub) = 0;
  virtual void set_clipboard_stub(ClipboardStub* clipboard_stub) = 0;
  virtual void set_video_renderer(VideoRenderer* video_renderer) = 0;

  // Initializes audio stream. Must be called before Connect().
  // |audio_decode_task_runner| will be used for audio decoding. |audio_stub|
  // will be called on the main thread.
  virtual void InitializeAudio(
      scoped_refptr<base::SingleThreadTaskRunner> audio_decode_task_runner,
      base::WeakPtr<AudioStub> audio_stub) = 0;

  // Initiates a connection using |session|. |event_callback| will be notified
  // of changes in the state of the connection and must outlive the
  // ConnectionToHost. Caller must set stubs (see below) before calling Connect.
  virtual void Connect(std::unique_ptr<Session> session,
                       scoped_refptr<TransportContext> transport_context,
                       HostEventCallback* event_callback) = 0;

  // Disconnects the host connection.
  virtual void Disconnect(ErrorCode error) = 0;

  // Returns the session configuration that was negotiated with the host.
  virtual const SessionConfig& config() = 0;

  // Stubs for sending data to the host.
  virtual ClipboardStub* clipboard_forwarder() = 0;
  virtual HostStub* host_stub() = 0;
  virtual InputStub* input_stub() = 0;

  // Return the current state of ConnectionToHost.
  virtual State state() const = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CONNECTION_TO_HOST_H_
