// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_MESSAGES_H_
#define REMOTING_HOST_CHROMOTING_MESSAGES_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_platform_file.h"
#include "remoting/host/chromoting_param_traits.h"
#include "remoting/proto/control.pb.h"
#include "remoting/protocol/file_transfer_helpers.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"

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
// Chromoting messages sent from the desktop to the network process.

// Notifies the network process that the host display layout has changed.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_DisplayChanged,
                    remoting::protocol::VideoLayout /* layout */)

// Carries a cursor share update from the desktop session agent to the client.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_MouseCursor,
                    webrtc::MouseCursor /* cursor */)

// Notifies the network process that the active keyboard layout has changed.
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_KeyboardChanged,
                    remoting::protocol::KeyboardLayout /* layout */)

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
IPC_MESSAGE_CONTROL(ChromotingDesktopNetworkMsg_FileDataResult,
                    uint64_t /* file_id */,
                    remoting::protocol::FileTransferResult<
                        std::vector<std::uint8_t>> /* result */)

//-----------------------------------------------------------------------------
// Chromoting messages sent from the network to the desktop process.

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
                    std::vector<std::uint8_t> /* data */)

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
