// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_mac.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <mach/mach_time.h>
#include <string>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "media/midi/midi_service.h"
#include "media/midi/task_service.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"

using base::NumberToString;
using base::SysCFStringRefToUTF8;
using midi::mojom::PortState;
using midi::mojom::Result;

// NB: System MIDI types are pointer types in 32-bit and integer types in
// 64-bit. Therefore, the initialization is the simplest one that satisfies both
// (if possible).

namespace midi {

namespace {

// Maximum buffer size that CoreMIDI can handle for MIDIPacketList.
const size_t kCoreMIDIMaxPacketListSize = 65536;
// Pessimistic estimation on available data size of MIDIPacketList.
const size_t kEstimatedMaxPacketDataSize = kCoreMIDIMaxPacketListSize / 2;

enum {
  kSessionTaskRunner = TaskService::kDefaultRunnerId,
  kClientTaskRunner,
};

mojom::PortInfo GetPortInfoFromEndpoint(MIDIEndpointRef endpoint) {
  std::string manufacturer;
  CFStringRef manufacturer_ref = NULL;
  OSStatus result = MIDIObjectGetStringProperty(
      endpoint, kMIDIPropertyManufacturer, &manufacturer_ref);
  if (result == noErr) {
    manufacturer = SysCFStringRefToUTF8(manufacturer_ref);
  } else {
    // kMIDIPropertyManufacturer is not supported in IAC driver providing
    // endpoints, and the result will be kMIDIUnknownProperty (-10835).
    DLOG(WARNING) << "Failed to get kMIDIPropertyManufacturer with status "
                  << result;
  }

  std::string name;
  CFStringRef name_ref = NULL;
  result = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName,
                                       &name_ref);
  if (result == noErr) {
    name = SysCFStringRefToUTF8(name_ref);
  } else {
    DLOG(WARNING) << "Failed to get kMIDIPropertyDisplayName with status "
                  << result;
  }

  std::string version;
  SInt32 version_number = 0;
  result = MIDIObjectGetIntegerProperty(
      endpoint, kMIDIPropertyDriverVersion, &version_number);
  if (result == noErr) {
    version = NumberToString(version_number);
  } else {
    // kMIDIPropertyDriverVersion is not supported in IAC driver providing
    // endpoints, and the result will be kMIDIUnknownProperty (-10835).
    DLOG(WARNING) << "Failed to get kMIDIPropertyDriverVersion with status "
                  << result;
  }

  std::string id;
  SInt32 id_number = 0;
  result = MIDIObjectGetIntegerProperty(
      endpoint, kMIDIPropertyUniqueID, &id_number);
  if (result == noErr) {
    id = NumberToString(id_number);
  } else {
    // On connecting some devices, e.g., nano KONTROL2, unknown endpoints
    // appear and disappear quickly and they fail on queries.
    // Let's ignore such ghost devices.
    // Same problems will happen if the device is disconnected before finishing
    // all queries.
    DLOG(WARNING) << "Failed to get kMIDIPropertyUniqueID with status "
                  << result;
  }

  const PortState state = PortState::OPENED;
  return mojom::PortInfo(id, manufacturer, name, version, state);
}

base::TimeTicks MIDITimeStampToTimeTicks(MIDITimeStamp timestamp) {
  return base::TimeTicks::FromMachAbsoluteTime(timestamp);
}

MIDITimeStamp TimeTicksToMIDITimeStamp(base::TimeTicks ticks) {
  // time.h doesn't yet support the opposite function for FromMachAbsoluteTime.
  // Instead, adapted from CAHostTimeBase.h in the Core Audio Utility Classes.
  struct mach_timebase_info base_time_info;
  mach_timebase_info(&base_time_info);
#if defined(ARCH_CPU_64_BITS)
  absl::uint128 result = ticks.since_origin().InNanoseconds();
#else
  long double result = ticks.since_origin().InNanoseconds();
#endif
  if (base_time_info.numer != base_time_info.denom) {
    result *= base_time_info.denom;
    result /= base_time_info.numer;
  }
  return static_cast<uint64_t>(result);
}

}  // namespace

MidiManager* MidiManager::Create(MidiService* service) {
  return new MidiManagerMac(service);
}

MidiManagerMac::MidiManagerMac(MidiService* service) : MidiManager(service) {}

MidiManagerMac::~MidiManagerMac() {
  if (!service()->task_service()->UnbindInstance())
    return;

  // Finalization steps should be implemented after the UnbindInstance() call.
  // Do not need to dispose |coremidi_input_| and |coremidi_output_| explicitly.
  // CoreMIDI automatically disposes them on the client disposal.
  base::AutoLock lock(midi_client_lock_);
  if (midi_client_)
    MIDIClientDispose(midi_client_);
}

void MidiManagerMac::StartInitialization() {
  if (!service()->task_service()->BindInstance())
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  service()->task_service()->PostBoundTask(
      kClientTaskRunner, base::BindOnce(&MidiManagerMac::InitializeCoreMIDI,
                                        base::Unretained(this)));
}

void MidiManagerMac::DispatchSendMidiData(MidiManagerClient* client,
                                          uint32_t port_index,
                                          const std::vector<uint8_t>& data,
                                          base::TimeTicks timestamp) {
  service()->task_service()->PostBoundTask(
      kClientTaskRunner,
      base::BindOnce(&MidiManagerMac::SendMidiData, base::Unretained(this),
                     client, port_index, data, timestamp));
}

void MidiManagerMac::InitializeCoreMIDI() {
  DCHECK(service()->task_service()->IsOnTaskRunner(kClientTaskRunner));

  // CoreMIDI registration.
  MIDIClientRef client = 0u;
  OSStatus result = MIDIClientCreate(CFSTR("Chrome"), ReceiveMidiNotifyDispatch,
                                     this, &client);
  if (result != noErr || client == 0u)
    return CompleteCoreMIDIInitialization(Result::INITIALIZATION_ERROR);

  {
    base::AutoLock lock(midi_client_lock_);
    midi_client_ = client;
  }

  // Create input and output port. These MIDIPortRef references are not needed
  // to be disposed explicitly. CoreMIDI automatically disposes them on the
  // client disposal.
  result = MIDIInputPortCreate(client, CFSTR("MIDI Input"), ReadMidiDispatch,
                               this, &midi_input_);
  if (result != noErr || midi_input_ == 0u)
    return CompleteCoreMIDIInitialization(Result::INITIALIZATION_ERROR);

  result = MIDIOutputPortCreate(client, CFSTR("MIDI Output"), &midi_output_);
  if (result != noErr || midi_output_ == 0u)
    return CompleteCoreMIDIInitialization(Result::INITIALIZATION_ERROR);

  // Following loop may miss some newly attached devices, but such device will
  // be captured by ReceiveMidiNotifyDispatch callback.
  destinations_.resize(MIDIGetNumberOfDestinations());
  for (size_t i = 0u; i < destinations_.size(); ++i) {
    MIDIEndpointRef destination = MIDIGetDestination(i);
    DCHECK_NE(0u, destination);

    // Keep track of all destinations (known as outputs by the Web MIDI API).
    destinations_[i] = destination;
    AddOutputPort(GetPortInfoFromEndpoint(destination));
  }
  // Allocate maximum size of buffer that CoreMIDI can handle.
  midi_buffer_.resize(kCoreMIDIMaxPacketListSize);

  // Open connections from all sources. This loop also may miss new devices.
  sources_.resize(MIDIGetNumberOfSources());
  for (size_t i = 0u; i < sources_.size(); ++i) {
    MIDIEndpointRef source = MIDIGetSource(i);
    DCHECK_NE(0u, source);

    // Keep track of all sources (known as inputs by the Web MIDI API).
    sources_[i] = source;
    AddInputPort(GetPortInfoFromEndpoint(source));
  }
  // Start listening.
  for (size_t i = 0u; i < sources_.size(); ++i)
    MIDIPortConnectSource(midi_input_, sources_[i], reinterpret_cast<void*>(i));

  CompleteCoreMIDIInitialization(Result::OK);
}

void MidiManagerMac::CompleteCoreMIDIInitialization(mojom::Result result) {
  service()->task_service()->PostBoundTask(
      kSessionTaskRunner,
      base::BindOnce(&MidiManagerMac::CompleteInitialization,
                     base::Unretained(this), result));
}

// static
void MidiManagerMac::ReceiveMidiNotifyDispatch(const MIDINotification* message,
                                               void* refcon) {
  // This callback function is invoked on |kClientTaskRunner|.
  // |manager| should be valid because we can ensure |midi_client_| is still
  // alive here.
  MidiManagerMac* manager = static_cast<MidiManagerMac*>(refcon);
  manager->ReceiveMidiNotify(message);
}

void MidiManagerMac::ReceiveMidiNotify(const MIDINotification* message) {
  DCHECK(service()->task_service()->IsOnTaskRunner(kClientTaskRunner));

  if (kMIDIMsgObjectAdded == message->messageID) {
    // New device is going to be attached.
    const MIDIObjectAddRemoveNotification* notification =
        reinterpret_cast<const MIDIObjectAddRemoveNotification*>(message);
    MIDIEndpointRef endpoint =
        static_cast<MIDIEndpointRef>(notification->child);
    if (notification->childType == kMIDIObjectType_Source) {
      // Attaching device is an input device.
      auto it = base::ranges::find(sources_, endpoint);
      if (it == sources_.end()) {
        mojom::PortInfo info = GetPortInfoFromEndpoint(endpoint);
        // If the device disappears before finishing queries, mojom::PortInfo
        // becomes incomplete. Skip and do not cache such information here.
        // On kMIDIMsgObjectRemoved, the entry will be ignored because it
        // will not be found in the pool.
        if (!info.id.empty()) {
          sources_.push_back(endpoint);
          AddInputPort(info);
          MIDIPortConnectSource(midi_input_, endpoint,
                                reinterpret_cast<void*>(sources_.size() - 1));
        }
      } else {
        SetInputPortState(it - sources_.begin(), PortState::OPENED);
      }
    } else if (notification->childType == kMIDIObjectType_Destination) {
      // Attaching device is an output device.
      auto it = base::ranges::find(destinations_, endpoint);
      if (it == destinations_.end()) {
        mojom::PortInfo info = GetPortInfoFromEndpoint(endpoint);
        // Skip cases that queries are not finished correctly.
        if (!info.id.empty()) {
          destinations_.push_back(endpoint);
          AddOutputPort(info);
        }
      } else {
        SetOutputPortState(it - destinations_.begin(), PortState::OPENED);
      }
    }
  } else if (kMIDIMsgObjectRemoved == message->messageID) {
    // Existing device is going to be detached.
    const MIDIObjectAddRemoveNotification* notification =
        reinterpret_cast<const MIDIObjectAddRemoveNotification*>(message);
    MIDIEndpointRef endpoint =
        static_cast<MIDIEndpointRef>(notification->child);
    if (notification->childType == kMIDIObjectType_Source) {
      // Detaching device is an input device.
      auto it = base::ranges::find(sources_, endpoint);
      if (it != sources_.end())
        SetInputPortState(it - sources_.begin(), PortState::DISCONNECTED);
    } else if (notification->childType == kMIDIObjectType_Destination) {
      // Detaching device is an output device.
      auto it = base::ranges::find(destinations_, endpoint);
      if (it != destinations_.end())
        SetOutputPortState(it - destinations_.begin(), PortState::DISCONNECTED);
    }
  }
}

// static
void MidiManagerMac::ReadMidiDispatch(const MIDIPacketList* packet_list,
                                      void* read_proc_refcon,
                                      void* src_conn_refcon) {
  // This method is called on a separate high-priority thread owned by CoreMIDI.
  // |manager| should be valid because we can ensure |midi_client_| is still
  // alive here.
  MidiManagerMac* manager = static_cast<MidiManagerMac*>(read_proc_refcon);
  DCHECK(manager);
  uint32_t port_index = reinterpret_cast<uintptr_t>(src_conn_refcon);

  // Go through each packet and process separately.
  const MIDIPacket* packet = &packet_list->packet[0];
  for (size_t i = 0u; i < packet_list->numPackets; i++) {
    // Each packet contains MIDI data for one or more messages (like note-on).
    base::TimeTicks timestamp = MIDITimeStampToTimeTicks(packet->timeStamp);

    manager->ReceiveMidiData(port_index, packet->data, packet->length,
                             timestamp);

    packet = MIDIPacketNext(packet);
  }
}

void MidiManagerMac::SendMidiData(MidiManagerClient* client,
                                  uint32_t port_index,
                                  const std::vector<uint8_t>& data,
                                  base::TimeTicks timestamp) {
  DCHECK(service()->task_service()->IsOnTaskRunner(kClientTaskRunner));

  // Lookup the destination based on the port index.
  if (static_cast<size_t>(port_index) >= destinations_.size())
    return;
  MIDITimeStamp coremidi_timestamp = TimeTicksToMIDITimeStamp(timestamp);
  MIDIEndpointRef destination = destinations_[port_index];

  size_t send_size;
  for (size_t sent_size = 0u; sent_size < data.size(); sent_size += send_size) {
    MIDIPacketList* packet_list =
        reinterpret_cast<MIDIPacketList*>(midi_buffer_.data());
    MIDIPacket* midi_packet = MIDIPacketListInit(packet_list);
    // Limit the maximum payload size to kEstimatedMaxPacketDataSize that is
    // half of midi_buffer data size. MIDIPacketList and MIDIPacket consume
    // extra buffer areas for meta information, and available size is smaller
    // than buffer size. Here, we simply assume that at least half size is
    // available for data payload.
    send_size = std::min(data.size() - sent_size, kEstimatedMaxPacketDataSize);
    midi_packet = MIDIPacketListAdd(
        packet_list,
        kCoreMIDIMaxPacketListSize,
        midi_packet,
        coremidi_timestamp,
        send_size,
        &data[sent_size]);
    DCHECK(midi_packet);

    MIDISend(midi_output_, destination, packet_list);
  }

  AccumulateMidiBytesSent(client, data.size());
}

}  // namespace midi
