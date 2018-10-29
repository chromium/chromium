// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MIDI_MESSAGES_H_
#define CONTENT_COMMON_MEDIA_MIDI_MESSAGES_H_

// IPC messages for access to MIDI hardware.

// TODO(toyoshim): Mojofication is working in progress. Until the work is
// finished, this file temporarily depends on midi_service.mojom.h.
// Once the migration is finished, this file will be removed.
// http://crbug.com/582327

#include <stdint.h>

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/midi/midi_service.mojom.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MidiMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(midi::mojom::PortState, midi::mojom::PortState::LAST)

IPC_STRUCT_TRAITS_BEGIN(midi::mojom::PortInfo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(manufacturer)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(state)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(midi::mojom::Result, midi::mojom::Result::MAX)

// Messages for IPC between MidiMessageFilter and MidiHost.

// Renderer request to browser for access to MIDI services.
IPC_MESSAGE_CONTROL0(MidiHostMsg_StartSession)

IPC_MESSAGE_CONTROL3(MidiHostMsg_SendData,
                     uint32_t /* port */,
                     std::vector<uint8_t> /* data */,
                     base::TimeTicks /* timestamp */)

IPC_MESSAGE_CONTROL0(MidiHostMsg_EndSession)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL1(MidiMsg_AddInputPort,
                     midi::mojom::PortInfo /* input port */)

IPC_MESSAGE_CONTROL1(MidiMsg_AddOutputPort,
                     midi::mojom::PortInfo /* output port */)

IPC_MESSAGE_CONTROL2(MidiMsg_SetInputPortState,
                     uint32_t /* port */,
                     midi::mojom::PortState /* state */)

IPC_MESSAGE_CONTROL2(MidiMsg_SetOutputPortState,
                     uint32_t /* port */,
                     midi::mojom::PortState /* state */)

IPC_MESSAGE_CONTROL1(MidiMsg_SessionStarted, midi::mojom::Result /* result */)

IPC_MESSAGE_CONTROL3(MidiMsg_DataReceived,
                     uint32_t /* port */,
                     std::vector<uint8_t> /* data */,
                     base::TimeTicks /* timestamp */)

IPC_MESSAGE_CONTROL1(MidiMsg_AcknowledgeSentData, uint32_t /* bytes sent */)

#endif  // CONTENT_COMMON_MEDIA_MIDI_MESSAGES_H_
