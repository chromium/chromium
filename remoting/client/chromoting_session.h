// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_SESSION_H_
#define REMOTING_CLIENT_CHROMOTING_SESSION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_telemetry_logger.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/connect_to_host_info.h"
#include "remoting/client/feedback_data.h"
#include "remoting/client/input/client_input_injector.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/signaling/ftl_device_id_provider.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

namespace protocol {
class AudioStub;
class VideoRenderer;
}  // namespace protocol

class ChromotingClientRuntime;

// ChromotingSession is scoped to the session.
// Construction, destruction, and all method calls must occur on the UI Thread.
// All callbacks will be posted to the UI Thread.
// A ChromotingSession instance can be used for at most one connection attempt.
// If you need to reconnect an ended session, you will need to create a new
// session instance.
class ChromotingSession : public ClientInputInjector {
 public:
  // All methods of the delegate are called on the UI thread. Callbacks should
  // also be invoked on the UI thread too.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Notifies the delegate of the current connection status. The delegate
    // should destroy the ChromotingSession instance when the connection state
    // is an end state.
    virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                   protocol::ErrorCode error) = 0;

    // Saves new pairing credentials to permanent storage.
    virtual void CommitPairingCredentials(const std::string& host,
                                          const std::string& id,
                                          const std::string& secret) = 0;

    // Notifies the user interface that the user needs to enter a PIN. The
    // current authentication attempt is put on hold until |callback| is
    // invoked.
    virtual void FetchSecret(
        bool pairing_supported,
        const protocol::SecretFetchedCallback& secret_fetched_callback) = 0;

    // Pass on the set of negotiated capabilities to the client.
    virtual void SetCapabilities(const std::string& capabilities) = 0;

    // Passes on the deconstructed ExtensionMessage to the client to handle
    // appropriately.
    virtual void HandleExtensionMessage(const std::string& type,
                                        const std::string& message) = 0;
  };

  using GetFeedbackDataCallback =
      base::OnceCallback<void(std::unique_ptr<FeedbackData>)>;

  // Initiates a connection with the specified host. This will start the
  // connection immediately.
  ChromotingSession(base::WeakPtr<ChromotingSession::Delegate> delegate,
                    std::unique_ptr<protocol::CursorShapeStub> cursor_stub,
                    std::unique_ptr<protocol::VideoRenderer> video_renderer,
                    std::unique_ptr<protocol::AudioStub> audio_player,
                    const ConnectToHostInfo& info);

  ChromotingSession(const ChromotingSession&) = delete;
  ChromotingSession& operator=(const ChromotingSession&) = delete;

  ~ChromotingSession() override;

  // Gets the current feedback data and returns it to the callback on the
  // UI thread.
  void GetFeedbackData(GetFeedbackDataCallback callback) const;

  // Requests pairing between the host and client for PIN-less authentication.
  void RequestPairing(const std::string& device_name);

  // Moves the host's cursor to the specified coordinates, optionally with some
  // mouse button depressed. If |button| is BUTTON_UNDEFINED, no click is made.
  void SendMouseEvent(int x,
                      int y,
                      protocol::MouseEvent_MouseButton button,
                      bool button_down);
  void SendMouseWheelEvent(int delta_x, int delta_y);

  //  ClientInputInjector implementation.
  bool SendKeyEvent(int scan_code, int key_code, bool key_down) override;
  void SendTextEvent(const std::string& text) override;

  // Sends the provided touch event payload to the host.
  void SendTouchEvent(const protocol::TouchEvent& touch_event);

  void SendClientResolution(int width_pixels, int height_pixels, float scale);

  // Enables or disables the video channel.
  void EnableVideoChannel(bool enable);

  void SendClientMessage(const std::string& type, const std::string& data);

 private:
  class Core;

  template <typename Functor, typename... Args>
  void RunCoreTaskOnNetworkThread(const base::Location& from_here,
                                  Functor&& core_functor,
                                  Args&&... args);

  // Used to obtain task runner references.
  const raw_ptr<ChromotingClientRuntime> runtime_;

  // Created when the session is connected, then used, and destroyed on the
  // network thread when the instance is destroyed.
  std::unique_ptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_SESSION_H_
