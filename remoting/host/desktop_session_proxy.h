// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_SESSION_PROXY_H_
#define REMOTING_HOST_DESKTOP_SESSION_PROXY_H_

#include <cstdint>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner_helpers.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "remoting/host/action_executor.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/file_transfer/ipc_file_operations.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/errors.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class ChannelProxy;
class Message;
}  // namespace IPC

namespace webrtc {
class MouseCursor;
}  // namespace webrtc

struct SerializedDesktopFrame;

namespace remoting {

class AudioPacket;
class ClientSessionControl;
class DesktopSessionConnector;
struct DesktopSessionProxyTraits;
class IpcAudioCapturer;
class IpcMouseCursorMonitor;
class IpcVideoFrameCapturer;
class ScreenControls;

// DesktopSessionProxy is created by an owning DesktopEnvironment to route
// requests from stubs to the DesktopSessionAgent instance through
// the IPC channel. DesktopSessionProxy is owned both by the DesktopEnvironment
// and the stubs, since stubs can out-live their DesktopEnvironment.
//
// DesktopSessionProxy objects are ref-counted but are always deleted on
// the |caller_task_runner_| thread. This makes it possible to continue
// to receive IPC messages after the ref-count has dropped to zero, until
// the proxy is deleted. DesktopSessionProxy must therefore avoid creating new
// references to the itself while handling IPC messages and desktop
// attach/detach notifications.
//
// All public methods of DesktopSessionProxy are called on
// the |caller_task_runner_| thread unless it is specified otherwise.
class DesktopSessionProxy
    : public base::RefCountedThreadSafe<DesktopSessionProxy,
                                        DesktopSessionProxyTraits>,
      public IPC::Listener,
      public IpcFileOperations::RequestHandler {
 public:
  DesktopSessionProxy(
      scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control,
      base::WeakPtr<DesktopSessionConnector> desktop_session_connector,
      const DesktopEnvironmentOptions& options);

  // Mirrors DesktopEnvironment.
  std::unique_ptr<ActionExecutor> CreateActionExecutor();
  std::unique_ptr<AudioCapturer> CreateAudioCapturer();
  std::unique_ptr<InputInjector> CreateInputInjector();
  std::unique_ptr<ScreenControls> CreateScreenControls();
  std::unique_ptr<webrtc::DesktopCapturer> CreateVideoCapturer();
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor();
  std::unique_ptr<FileOperations> CreateFileOperations();
  std::string GetCapabilities() const;
  void SetCapabilities(const std::string& capabilities);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Connects to the desktop session agent.
  bool AttachToDesktop(const IPC::ChannelHandle& desktop_pipe, int session_id);

  // Closes the connection to the desktop session agent and cleans up
  // the associated resources.
  void DetachFromDesktop();

  // Disconnects the client session that owns |this|.
  void DisconnectSession(protocol::ErrorCode error);

  // Stores |audio_capturer| to be used to post captured audio packets. Called
  // on the |audio_capture_task_runner_| thread.
  void SetAudioCapturer(const base::WeakPtr<IpcAudioCapturer>& audio_capturer);

  // APIs used to implement the webrtc::DesktopCapturer interface. These must be
  // called on the |video_capture_task_runner_| thread.
  void CaptureFrame();
  bool SelectSource(webrtc::DesktopCapturer::SourceId id);

  // Stores |video_capturer| to be used to post captured video frames. Called on
  // the |video_capture_task_runner_| thread.
  void SetVideoCapturer(
      const base::WeakPtr<IpcVideoFrameCapturer> video_capturer);

  // Stores |mouse_cursor_monitor| to be used to post mouse cursor changes.
  // Called on the |video_capture_task_runner_| thread.
  void SetMouseCursorMonitor(
      const base::WeakPtr<IpcMouseCursorMonitor>& mouse_cursor_monitor);

  // APIs used to implement the InputInjector interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event);
  void InjectKeyEvent(const protocol::KeyEvent& event);
  void InjectTextEvent(const protocol::TextEvent& event);
  void InjectMouseEvent(const protocol::MouseEvent& event);
  void InjectTouchEvent(const protocol::TouchEvent& event);
  void StartInputInjector(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard);

  // API used to implement the SessionController interface.
  void SetScreenResolution(const ScreenResolution& resolution);

  // API used to implement the ActionExecutor interface.
  void ExecuteAction(const protocol::ActionRequest& request);

  // IpcFileOperations::RequestHandler implementation.
  void ReadFile(std::uint64_t file_id) override;
  void ReadChunk(std::uint64_t file_id, std::uint64_t size) override;
  void WriteFile(std::uint64_t file_id,
                 const base::FilePath& filename) override;
  void WriteChunk(std::uint64_t file_id, std::string data) override;
  void Close(std::uint64_t file_id) override;
  void Cancel(std::uint64_t file_id) override;

  uint32_t desktop_session_id() const { return desktop_session_id_; }

 private:
  friend class base::DeleteHelper<DesktopSessionProxy>;
  friend struct DesktopSessionProxyTraits;

  class IpcSharedBufferCore;
  class IpcSharedBuffer;
  typedef std::map<int, scoped_refptr<IpcSharedBufferCore> > SharedBuffers;

  ~DesktopSessionProxy() override;

  // Returns a shared buffer from the list of known buffers.
  scoped_refptr<IpcSharedBufferCore> GetSharedBufferCore(int id);

  // Handles AudioPacket notification from the desktop session agent.
  void OnAudioPacket(const std::string& serialized_packet);

  // Registers a new shared buffer created by the desktop process.
  void OnCreateSharedBuffer(int id,
                            base::ReadOnlySharedMemoryRegion region,
                            uint32_t size);

  // Drops a cached reference to the shared buffer.
  void OnReleaseSharedBuffer(int id);

  // Handles DesktopDisplayChange notification from the desktop session agent.
  void OnDesktopDisplayChanged(const protocol::VideoLayout& layout);

  // Handles CaptureResult notification from the desktop session agent.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       const SerializedDesktopFrame& serialized_frame);

  // Handles MouseCursor notification from the desktop session agent.
  void OnMouseCursor(const webrtc::MouseCursor& mouse_cursor);

  // Handles InjectClipboardEvent request from the desktop integration process.
  void OnInjectClipboardEvent(const std::string& serialized_event);

  // Sends a message to the desktop session agent. The message is silently
  // deleted if the channel is broken.
  void SendToDesktop(IPC::Message* message);

  // Task runners:
  //   - |audio_capturer_| is called back on |audio_capture_task_runner_|.
  //   - public methods of this class (with some exceptions) are called on
  //     |caller_task_runner_|.
  //   - background I/O is served on |io_task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> audio_capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // Points to the audio capturer receiving captured audio packets.
  base::WeakPtr<IpcAudioCapturer> audio_capturer_;

  // Points to the client stub passed to StartInputInjector().
  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // Used to create a desktop session and receive notifications every time
  // the desktop process is replaced.
  base::WeakPtr<DesktopSessionConnector> desktop_session_connector_;

  // Points to the video capturer receiving captured video frames.
  base::WeakPtr<IpcVideoFrameCapturer> video_capturer_;

  // Points to the mouse cursor monitor receiving mouse cursor changes.
  base::WeakPtr<IpcMouseCursorMonitor> mouse_cursor_monitor_;

  // Used to create IpcFileOperations instances and route result messages.
  IpcFileOperationsFactory ipc_file_operations_factory_;

  // IPC channel to the desktop session agent.
  std::unique_ptr<IPC::ChannelProxy> desktop_channel_;

  int pending_capture_frame_requests_;

  // Shared memory buffers by Id. Each buffer is owned by the corresponding
  // frame.
  SharedBuffers shared_buffers_;

  // Keeps the desired screen resolution so it can be passed to a newly attached
  // desktop session agent.
  ScreenResolution screen_resolution_;

  // True if |this| has been connected to the desktop session.
  bool is_desktop_session_connected_;

  DesktopEnvironmentOptions options_;

  // Stores the session id for the proxied desktop process.
  uint32_t desktop_session_id_ = UINT32_MAX;

  DISALLOW_COPY_AND_ASSIGN(DesktopSessionProxy);
};

// Destroys |DesktopSessionProxy| instances on the caller's thread.
struct DesktopSessionProxyTraits {
  static void Destruct(const DesktopSessionProxy* desktop_session_proxy);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_SESSION_PROXY_H_
