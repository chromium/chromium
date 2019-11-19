// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_MESSAGES_H_
#define REMOTING_HOST_CHROMOTING_MESSAGES_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/chromoting_param_traits.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/action.pb.h"
#include "remoting/proto/process_stats.pb.h"
#include "remoting/protocol/errors.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "remoting/protocol/transport.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

#endif  // REMOTING_HOST_CHROMOTING_MESSAGES_H_

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChromotingMsgStart

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon.

// Requests the receiving process to crash producing a crash dump. The daemon
// sends this message when a fatal error has been detected indicating that
// the receiving process misbehaves. The daemon passes the location of the code
// that detected the error.
IPC_MESSAGE_CONTROL(ChromotingDaemonMsg_Crash,
                    std::string /* function_name */,
                    std::string /* file_name */,
                    int /* line_number */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon to the network process.

// Delivers the host configuration (and updates) to the network process.
IPC_MESSAGE_CONTROL(ChromotingDaemonNetworkMsg_Configuration, std::string)

// Initializes the pairing registry on Windows. The passed key handles are
// already duplicated by the sender.
IPC_MESSAGE_CONTROL(ChromotingDaemonNetworkMsg_InitializePairingRegistry,
                    IPC::PlatformFileForTransit /* privileged_key */,
                    IPC::PlatformFileForTransit /* unprivileged_key */)

// Notifies the network process that the terminal |terminal_id| has been
// disconnected from the desktop session.
IPC_MESSAGE_CONTROL(ChromotingDaemonNetworkMsg_TerminalDisconnected,
                    int /* terminal_id */)

// Notifies the network process that |terminal_id| is now attached to
// a desktop integration process. |session_id| is the id of the desktop session
// being attached. |desktop_pipe| is the client end of the desktop-to-network
// pipe opened.
IPC_MESSAGE_CONTROL(ChromotingDaemonNetworkMsg_DesktopAttached,
                    int /* terminal_id */,
                    int /* session_id */,
                    IPC::ChannelHandle /* desktop_pipe */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the daemon process.

// Connects the terminal |terminal_id| (i.e. a remote client) to a desktop
// session.
IPC_MESSAGE_CONTROL(ChromotingNetworkHostMsg_ConnectTerminal,
                    int /* terminal_id */,
                    remoting::ScreenResolution /* resolution */,
                    bool /* virtual_terminal */)

// Disconnects the terminal |terminal_id| from the desktop session it was
// connected to.
IPC_MESSAGE_CONTROL(ChromotingNetworkHostMsg_DisconnectTerminal,
                    int /* terminal_id */)

// Changes the screen resolution in the given desktop session.
IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_SetScreenResolution,
                    int /* terminal_id */,
                    remoting::ScreenResolution /* resolution */)

// Serialized remoting::protocol::TransportRoute structure.
IPC_STRUCT_BEGIN(SerializedTransportRoute)
  IPC_STRUCT_MEMBER(remoting::protocol::TransportRoute::RouteType, type)
  IPC_STRUCT_MEMBER(std::vector<uint8_t>, remote_ip)
  IPC_STRUCT_MEMBER(uint16_t, remote_port)
  IPC_STRUCT_MEMBER(std::vector<uint8_t>, local_ip)
  IPC_STRUCT_MEMBER(uint16_t, local_port)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(remoting::protocol::TransportRoute::RouteType,
                          remoting::protocol::TransportRoute::ROUTE_TYPE_MAX)

// Hosts status notifications (see HostStatusObserver interface) sent by
// IpcHostEventLogger.
IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_AccessDenied,
                    std::string /* jid */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_ClientAuthenticated,
                    std::string /* jid */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_ClientConnected,
                    std::string /* jid */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_ClientDisconnected,
                    std::string /* jid */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_ClientRouteChange,
                    std::string /* jid */,
                    std::string /* channel_name */,
                    SerializedTransportRoute /* route */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_HostStarted,
                    std::string /* xmpp_login */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_HostShutdown)

// Instructs the daemon process to update the config file, replacing the current
// OAuth refresh token with the one provided.
IPC_MESSAGE_CONTROL(ChromotingNetworkDaemonMsg_UpdateConfigRefreshToken,
                    std::string /* token */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the daemon process.

// Notifies the daemon that a desktop integration process has been initialized.
// |desktop_pipe| specifies the client end of the desktop pipe. It is to be
// forwarded to the desktop environment stub.
IPC_MESSAGE_CONTROL(ChromotingDesktopDaemonMsg_DesktopAttached,
                    IPC::ChannelHandle /* desktop_pipe */)

// Asks the daemon to inject Secure Attention Sequence (SAS) in the session
// where the desktop process is running.
IPC_MESSAGE_CONTROL(ChromotingDesktopDaemonMsg_InjectSas)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the network process.

// Notifies the network process that a shared buffer has been created.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_CreateSharedBuffer,
                    int /* id */,
                    base::ReadOnlySharedMemoryRegion /* region */,
                    uint32_t /* size */)

// Request the network process to stop using a shared buffer.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_ReleaseSharedBuffer,
                    int /* id */)

// Serialized webrtc::DesktopFrame.
IPC_STRUCT_BEGIN(SerializedDesktopFrame)
  // ID of the shared memory buffer containing the pixels.
  IPC_STRUCT_MEMBER(int, shared_buffer_id)

  // Width of a single row of pixels in bytes.
  IPC_STRUCT_MEMBER(int, bytes_per_row)

  // Captured region.
  IPC_STRUCT_MEMBER(std::vector<webrtc::DesktopRect>, dirty_region)

  // Dimensions of the buffer in pixels.
  IPC_STRUCT_MEMBER(webrtc::DesktopSize, dimensions)

  // Time spent in capture. Unit is in milliseconds.
  IPC_STRUCT_MEMBER(int64_t, capture_time_ms)

  // Latest event timestamp supplied by the client for performance tracking.
  IPC_STRUCT_MEMBER(int64_t, latest_event_timestamp)

  // DPI for this frame.
  IPC_STRUCT_MEMBER(webrtc::DesktopVector, dpi)

  // Capturer Id
  IPC_STRUCT_MEMBER(uint32_t, capturer_id)
IPC_STRUCT_END()

IPC_ENUM_TRAITS_MAX_VALUE(webrtc::DesktopCapturer::Result,
                          webrtc::DesktopCapturer::Result::MAX_VALUE)

// Notifies the network process that a shared buffer has been created.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_CaptureResult,
                    webrtc::DesktopCapturer::Result /* result */,
                    SerializedDesktopFrame /* frame */)

// Notifies the network process that the host display layout has changed.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_DisplayChanged,
                    remoting::protocol::VideoLayout /* layout */)

// Carries a cursor share update from the desktop session agent to the client.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_MouseCursor,
                    webrtc::MouseCursor /* cursor */)

// Carries a clipboard event from the desktop session agent to the client.
// |serialized_event| is a serialized protocol::ClipboardEvent.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_InjectClipboardEvent,
                    std::string /* serialized_event */)

IPC_ENUM_TRAITS_MAX_VALUE(remoting::protocol::ErrorCode,
                          remoting::protocol::ERROR_CODE_MAX)

// Requests the network process to terminate the client session.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_DisconnectSession,
                    remoting::protocol::ErrorCode /* error */)

// Carries an audio packet from the desktop session agent to the client.
// |serialized_packet| is a serialized AudioPacket.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_AudioPacket,
                    std::string /* serialized_packet */)

// Informs the network process of the result of a file operation on the file
// identified by |file_id|. If |result| is an error, the file ID is no longer
// valid.
IPC_MESSAGE_CONTROL(
    ChromotingDesktopNetworkMsg_FileResult,
    uint64_t /* file_id */,
    remoting::protocol::FileTransferResult<remoting::Monostate> /* result */)

// Carries the result of a read-file operation on the file identified by
// |file_id|. |result| is the filename and size of the selected file. If
// |result| is an error, the file ID is no longer valid.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_FileInfoResult,
                    uint64_t /* file_id */,
                    remoting::protocol::FileTransferResult<
                        std::tuple<base::FilePath, uint64_t>> /* result */)

// Carries the result of a file read-chunk operation on the file identified by
// |file_id|. |result| holds the read data. If |result| is an error, the file ID
// is no longer valid.
IPC_MESSAGE_CONTROL(
    ChromotingDesktopNetworkMsg_FileDataResult,
    uint64_t /* file_id */,
    remoting::protocol::FileTransferResult<std::string> /* result */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the desktop process.

// Passes the client session data to the desktop session agent and starts it.
// This must be the first message received from the host.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_StartSessionAgent,
                    std::string /* authenticated_jid */,
                    remoting::ScreenResolution /* resolution */,
                    remoting::DesktopEnvironmentOptions /* options */)

IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_CaptureFrame)

IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_SelectSource,
                    int /* desktop_display_id */)

// Carries a clipboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::ClipboardEvent.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_InjectClipboardEvent,
                    std::string /* serialized_event */)

// Carries a keyboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::KeyEvent.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_InjectKeyEvent,
                    std::string /* serialized_event */)

// Carries a keyboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::TextEvent.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_InjectTextEvent,
                    std::string /* serialized_event */)

// Carries a mouse event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::MouseEvent.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_InjectMouseEvent,
                    std::string /* serialized_event */)

// Carries a touch event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::TouchEvent.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_InjectTouchEvent,
                    std::string /* serialized_event */)

// Changes the screen resolution in the desktop session.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_SetScreenResolution,
                    remoting::ScreenResolution /* resolution */)

// Carries an action request event from the client to the desktop session agent.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_ExecuteActionRequest,
                    remoting::protocol::ActionRequest /* request */)

// Requests that the desktop process create a new file for writing with the
// provided file name, which will be identified by |file_id|. The desktop
// process will respond with a FileResult message.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_WriteFile,
                    uint64_t /* file_id */,
                    base::FilePath /* filename */)

// Requests that the desktop process append the provided data chunk to the
// previously created file identified by |file_id|. The desktop process will
// respond with a FileResult message.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_WriteFileChunk,
                    uint64_t /* file_id */,
                    std::string /* data */)

// Prompt the user to select a file for reading, which will be identified by
// |file_id|. The desktop process will respond with a FileInfoResult message.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_ReadFile,
                    uint64_t /* file_id */)

// Requests that the desktop process read a data chunk from the file identified
// by |file_id|. The desktop process will respond with a FileDataResult message.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_ReadFileChunk,
                    uint64_t /* file_id */,
                    uint64_t /* size */)

// Requests that the desktop process close the file identified by |file_id|.
// If the file is being written, it will be finalized, and the desktop process
// will respond with a FileResult message. If the file is being read, there is
// no response message.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_CloseFile,
                    uint64_t /* file_id */)

// Requests that the desktop process cancel the file identified by |file_id|.
// If the file is being written, the partial file will be deleted. If the file
// is being read, it will be closed. In either case, there is no response
// message.
IPC_MESSAGE_CONTROL(ChromotingNetworkDesktopMsg_CancelFile,
                    uint64_t /* file_id */)

//---------------------------------------------------------------------
// Chromoting messages sent from the remote_security_key process to the
// network process.

// The array of bytes representing a security key request to be sent to the
// remote client.
IPC_MESSAGE_CONTROL(ChromotingRemoteSecurityKeyToNetworkMsg_Request,
                    std::string /* request bytes */)

//---------------------------------------------------------
// Chromoting messages sent from the network process to the remote_security_key
// process.

// The array of bytes representing the security key response from the client.
IPC_MESSAGE_CONTROL(ChromotingNetworkToRemoteSecurityKeyMsg_Response,
                    std::string /* response bytes */)

// Indicates the channel used for security key message passing is ready for use.
IPC_MESSAGE_CONTROL(ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionReady)

// Error indicating the request originated from outside the remoted session.
// The IPC channel will be disconnected after this message has been sent.
IPC_MESSAGE_CONTROL(ChromotingNetworkToRemoteSecurityKeyMsg_InvalidSession)

// Starts to report process resource usage.
IPC_MESSAGE_CONTROL(ChromotingNetworkToAnyMsg_StartProcessStatsReport,
                    base::TimeDelta /* interval */)

// Stops to report process resource usage.
IPC_MESSAGE_CONTROL(ChromotingNetworkToAnyMsg_StopProcessStatsReport)

// Reports process resource usage to network process.
IPC_MESSAGE_CONTROL(ChromotingAnyToNetworkMsg_ReportProcessStats,
                    remoting::protocol::AggregatedProcessResourceUsage)
