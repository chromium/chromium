// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
#define REMOTING_HOST_DESKTOP_SESSION_AGENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/current_process_stats_agent.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/file_transfer/session_file_operations_handler.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/process_stats_stub.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "ui/events/event.h"

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace remoting {

class ActionExecutor;
class AudioCapturer;
class AudioPacket;
class AutoThreadTaskRunner;
class DesktopEnvironment;
class DesktopEnvironmentFactory;
class InputInjector;
class ProcessStatsSender;
class RemoteInputFilter;
class ScreenControls;
class ScreenResolution;

namespace protocol {
class ActionRequest;
class InputEventTracker;
}  // namespace protocol

// Provides screen/audio capturing and input injection services for
// the network process.
class DesktopSessionAgent
    : public base::RefCountedThreadSafe<DesktopSessionAgent>,
      public IPC::Listener,
      public webrtc::DesktopCapturer::Callback,
      public webrtc::MouseCursorMonitor::Callback,
      public ClientSessionControl,
      public protocol::ProcessStatsStub,
      public IpcFileOperations::ResultHandler {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Returns an instance of desktop environment factory used.
    virtual DesktopEnvironmentFactory& desktop_environment_factory() = 0;

    // Notifies the delegate that the network-to-desktop channel has been
    // disconnected.
    virtual void OnNetworkProcessDisconnected() = 0;
  };

  DesktopSessionAgent(
      scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> io_task_runner);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // webrtc::DesktopCapturer::Callback implementation.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // webrtc::MouseCursorMonitor::Callback implementation.
  void OnMouseCursor(webrtc::MouseCursor* cursor) override;
  void OnMouseCursorPosition(webrtc::MouseCursorMonitor::CursorState state,
                             const webrtc::DesktopVector& position) override;

  // Forwards a local clipboard event though the IPC channel to the network
  // process.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);

  // Forwards an audio packet though the IPC channel to the network process.
  void ProcessAudioPacket(std::unique_ptr<AudioPacket> packet);

  // IpcFileOperations::ResultHandler implementation.
  void OnResult(std::uint64_t file_id, ResultHandler::Result result) override;
  void OnInfoResult(std::uint64_t file_id,
                    ResultHandler::InfoResult result) override;
  void OnDataResult(std::uint64_t file_id,
                    ResultHandler::DataResult result) override;

  // Creates desktop integration components and a connected IPC channel to be
  // used to access them. The client end of the channel is returned.
  mojo::ScopedMessagePipeHandle Start(const base::WeakPtr<Delegate>& delegate);

  // Stops the agent asynchronously.
  void Stop();

 protected:
  friend class base::RefCountedThreadSafe<DesktopSessionAgent>;

  ~DesktopSessionAgent() override;

  // ClientSessionControl interface.
  const std::string& client_jid() const override;
  void DisconnectSession(protocol::ErrorCode error) override;
  void OnLocalKeyPressed(uint32_t usb_keycode) override;
  void OnLocalPointerMoved(const webrtc::DesktopVector& position,
                           ui::EventType type) override;
  void SetDisableInputs(bool disable_inputs) override;
  void OnDesktopDisplayChanged(
      std::unique_ptr<protocol::VideoLayout> layout) override;

  // ProcessStatsStub interface.
  void OnProcessStats(
      const protocol::AggregatedProcessResourceUsage& usage) override;

  // Handles StartSessionAgent request from the client.
  void OnStartSessionAgent(const std::string& authenticated_jid,
                           const ScreenResolution& resolution,
                           const DesktopEnvironmentOptions& options);

  // Handles CaptureFrame requests from the client.
  void OnCaptureFrame();

  // Handles desktop display selection requests from the client.
  void OnSelectSource(int id);

  // Handles event executor requests from the client.
  void OnInjectClipboardEvent(const std::string& serialized_event);
  void OnInjectKeyEvent(const std::string& serialized_event);
  void OnInjectTextEvent(const std::string& serialized_event);
  void OnInjectMouseEvent(const std::string& serialized_event);
  void OnInjectTouchEvent(const std::string& serialized_event);
  void OnExecuteActionRequestEvent(const protocol::ActionRequest& request);

  // Handles ChromotingNetworkDesktopMsg_SetScreenResolution request from
  // the client.
  void SetScreenResolution(const ScreenResolution& resolution);

  // Sends a message to the network process.
  void SendToNetwork(std::unique_ptr<IPC::Message> message);

  // Posted to |audio_capture_task_runner_| to start the audio capturer.
  void StartAudioCapturer();

  // Posted to |audio_capture_task_runner_| to stop the audio capturer.
  void StopAudioCapturer();

  // Starts to report process statistic data to network process. If
  // |interval| is less than or equal to 0, a default non-zero value will be
  // used.
  void StartProcessStatsReport(base::TimeDelta interval);

  // Stops sending process statistic data to network process.
  void StopProcessStatsReport();

 private:
  // Task runner dedicated to running methods of |audio_capturer_|.
  scoped_refptr<AutoThreadTaskRunner> audio_capture_task_runner_;

  // Task runner on which public methods of this class should be called.
  scoped_refptr<AutoThreadTaskRunner> caller_task_runner_;

  // Task runner on which keyboard/mouse input is injected.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner_;

  // Task runner used by the IPC channel.
  scoped_refptr<AutoThreadTaskRunner> io_task_runner_;

  // Captures audio output.
  std::unique_ptr<AudioCapturer> audio_capturer_;

  std::string client_jid_;

  base::WeakPtr<Delegate> delegate_;

  // The DesktopEnvironment instance used by this agent.
  std::unique_ptr<DesktopEnvironment> desktop_environment_;

  // Executes action request events.
  std::unique_ptr<ActionExecutor> action_executor_;

  // Executes keyboard, mouse and clipboard events.
  std::unique_ptr<InputInjector> input_injector_;

  // Tracker used to release pressed keys and buttons when disconnecting.
  std::unique_ptr<protocol::InputEventTracker> input_tracker_;

  // Filter used to disable remote inputs during local input activity.
  std::unique_ptr<RemoteInputFilter> remote_input_filter_;

  // Used to apply client-requested changes in screen resolution.
  std::unique_ptr<ScreenControls> screen_controls_;

  // IPC channel connecting the desktop process with the network process.
  std::unique_ptr<IPC::ChannelProxy> network_channel_;

  // True if the desktop session agent has been started.
  bool started_ = false;

  // Captures the screen.
  std::unique_ptr<webrtc::DesktopCapturer> video_capturer_;

  // Captures mouse shapes.
  std::unique_ptr<webrtc::MouseCursorMonitor> mouse_cursor_monitor_;

  // Keep reference to the last frame sent to make sure shared buffer is alive
  // before it's received.
  std::unique_ptr<webrtc::DesktopFrame> last_frame_;

  // Routes file-transfer messages to the corresponding reader/writer to be
  // executed.
  base::Optional<SessionFileOperationsHandler> session_file_operations_handler_;

  // Reports process statistic data to network process.
  std::unique_ptr<ProcessStatsSender> stats_sender_;

  CurrentProcessStatsAgent current_process_stats_;

  // Used to disable callbacks to |this|.
  base::WeakPtrFactory<DesktopSessionAgent> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionAgent);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_AGENT_H_
