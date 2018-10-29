// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_MESSAGES_H_
#define REMOTING_HOST_CHROMOTING_MESSAGES_H_

#include <stdint.h>

#include "base/memory/shared_memory_handle.h"
#include "base/time/time.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/chromoting_param_traits.h"
#include "remoting/host/desktop_environment_options.h"
#include "remoting/host/screen_resolution.h"
#include "remoting/proto/action.pb.h"
#include "remoting/proto/process_stats.pb.h"
#include "remoting/protocol/errors.h"
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
IPC_MESSAGE_CONTROL3(ChromotingDaemonMsg_Crash,
                     std::string /* function_name */,
                     std::string /* file_name */,
                     int /* line_number */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon to the network process.

// Delivers the host configuration (and updates) to the network process.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_Configuration, std::string)

// Initializes the pairing registry on Windows. The passed key handles are
// already duplicated by the sender.
IPC_MESSAGE_CONTROL2(ChromotingDaemonNetworkMsg_InitializePairingRegistry,
                     IPC::PlatformFileForTransit /* privileged_key */,
                     IPC::PlatformFileForTransit /* unprivileged_key */)

// Notifies the network process that the terminal |terminal_id| has been
// disconnected from the desktop session.
IPC_MESSAGE_CONTROL1(ChromotingDaemonNetworkMsg_TerminalDisconnected,
                     int /* terminal_id */)

// Notifies the network process that |terminal_id| is now attached to
// a desktop integration process. |session_id| is the id of the desktop session
// being attached. |desktop_pipe| is the client end of the desktop-to-network
// pipe opened.
IPC_MESSAGE_CONTROL3(ChromotingDaemonNetworkMsg_DesktopAttached,
                     int /* terminal_id */,
                     int /* session_id */,
                     IPC::ChannelHandle /* desktop_pipe */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the daemon process.

// Connects the terminal |terminal_id| (i.e. a remote client) to a desktop
// session.
IPC_MESSAGE_CONTROL3(ChromotingNetworkHostMsg_ConnectTerminal,
                     int /* terminal_id */,
                     remoting::ScreenResolution /* resolution */,
                     bool /* virtual_terminal */)

// Disconnects the terminal |terminal_id| from the desktop session it was
// connected to.
IPC_MESSAGE_CONTROL1(ChromotingNetworkHostMsg_DisconnectTerminal,
                     int /* terminal_id */)

// Changes the screen resolution in the given desktop session.
IPC_MESSAGE_CONTROL2(ChromotingNetworkDaemonMsg_SetScreenResolution,
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
IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_AccessDenied,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_ClientAuthenticated,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_ClientConnected,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_ClientDisconnected,
                     std::string /* jid */)

IPC_MESSAGE_CONTROL3(ChromotingNetworkDaemonMsg_ClientRouteChange,
                     std::string /* jid */,
                     std::string /* channel_name */,
                     SerializedTransportRoute /* route */)

IPC_MESSAGE_CONTROL1(ChromotingNetworkDaemonMsg_HostStarted,
                     std::string /* xmpp_login */)

IPC_MESSAGE_CONTROL0(ChromotingNetworkDaemonMsg_HostShutdown)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the daemon process.

// Notifies the daemon that a desktop integration process has been initialized.
// |desktop_pipe| specifies the client end of the desktop pipe. It is to be
// forwarded to the desktop environment stub.
IPC_MESSAGE_CONTROL1(ChromotingDesktopDaemonMsg_DesktopAttached,
                     IPC::ChannelHandle /* desktop_pipe */)

// Asks the daemon to inject Secure Attention Sequence (SAS) in the session
// where the desktop process is running.
IPC_MESSAGE_CONTROL0(ChromotingDesktopDaemonMsg_InjectSas)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the desktop to the network process.

// Notifies the network process that a shared buffer has been created.
IPC_MESSAGE_CONTROL3(ChromotingDesktopNetworkMsg_CreateSharedBuffer,
                     int /* id */,
                     base::SharedMemoryHandle /* handle */,
                     uint32_t /* size */)

// Request the network process to stop using a shared buffer.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_ReleaseSharedBuffer,
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
IPC_MESSAGE_CONTROL2(ChromotingDesktopNetworkMsg_CaptureResult,
                     webrtc::DesktopCapturer::Result /* result */,
                     SerializedDesktopFrame /* frame */)

// Carries a cursor share update from the desktop session agent to the client.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_MouseCursor,
                     webrtc::MouseCursor /* cursor */ )

// Carries a clipboard event from the desktop session agent to the client.
// |serialized_event| is a serialized protocol::ClipboardEvent.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_InjectClipboardEvent,
                     std::string /* serialized_event */ )

IPC_ENUM_TRAITS_MAX_VALUE(remoting::protocol::ErrorCode,
                          remoting::protocol::ERROR_CODE_MAX)

// Requests the network process to terminate the client session.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_DisconnectSession,
                     remoting::protocol::ErrorCode /* error */)

// Carries an audio packet from the desktop session agent to the client.
// |serialized_packet| is a serialized AudioPacket.
IPC_MESSAGE_CONTROL1(ChromotingDesktopNetworkMsg_AudioPacket,
                     std::string /* serialized_packet */ )

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the desktop process.

// Passes the client session data to the desktop session agent and starts it.
// This must be the first message received from the host.
IPC_MESSAGE_CONTROL3(ChromotingNetworkDesktopMsg_StartSessionAgent,
                     std::string /* authenticated_jid */,
                     remoting::ScreenResolution /* resolution */,
                     remoting::DesktopEnvironmentOptions /* options */)

IPC_MESSAGE_CONTROL0(ChromotingNetworkDesktopMsg_CaptureFrame)

// Carries a clipboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::ClipboardEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectClipboardEvent,
                     std::string /* serialized_event */ )

// Carries a keyboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::KeyEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectKeyEvent,
                     std::string /* serialized_event */ )

// Carries a keyboard event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::TextEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectTextEvent,
                     std::string /* serialized_event */ )

// Carries a mouse event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::MouseEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectMouseEvent,
                     std::string /* serialized_event */ )

// Carries a touch event from the client to the desktop session agent.
// |serialized_event| is a serialized protocol::TouchEvent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_InjectTouchEvent,
                     std::string /* serialized_event */ )

// Changes the screen resolution in the desktop session.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_SetScreenResolution,
                     remoting::ScreenResolution /* resolution */)

// Carries an action request event from the client to the desktop session agent.
IPC_MESSAGE_CONTROL1(ChromotingNetworkDesktopMsg_ExecuteActionRequest,
                     remoting::protocol::ActionRequest /* request */)

//---------------------------------------------------------------------
// Chromoting messages sent from the remote_security_key process to the
// network process.

// The array of bytes representing a security key request to be sent to the
// remote client.
IPC_MESSAGE_CONTROL1(ChromotingRemoteSecurityKeyToNetworkMsg_Request,
                     std::string /* request bytes */)

//---------------------------------------------------------
// Chromoting messages sent from the network process to the remote_security_key
// process.

// The array of bytes representing the security key response from the client.
IPC_MESSAGE_CONTROL1(ChromotingNetworkToRemoteSecurityKeyMsg_Response,
                     std::string /* response bytes */)

// Indicates the channel used for security key message passing is ready for use.
IPC_MESSAGE_CONTROL0(ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionReady)

// Error indicating the request originated from outside the remoted session.
// The IPC channel will be disconnected after this message has been sent.
IPC_MESSAGE_CONTROL0(ChromotingNetworkToRemoteSecurityKeyMsg_InvalidSession)

// Starts to report process resource usage.
IPC_MESSAGE_CONTROL1(ChromotingNetworkToAnyMsg_StartProcessStatsReport,
                     base::TimeDelta /* interval */)

// Stops to report process resource usage.
IPC_MESSAGE_CONTROL0(ChromotingNetworkToAnyMsg_StopProcessStatsReport)

// Reports process resource usage to network process.
IPC_MESSAGE_CONTROL1(ChromotingAnyToNetworkMsg_ReportProcessStats,
                     remoting::protocol::AggregatedProcessResourceUsage)
