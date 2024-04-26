// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "media/midi/midi_service.h"
#include "media/midi/midi_service.mojom.h"
#include "media/midi/task_service.h"

namespace midi {

namespace {

using mojom::PortState;
using mojom::Result;

enum {
  kDefaultRunnerNotUsedOnAlsa = TaskService::kDefaultRunnerId,
  kEventTaskRunner,
  kSendTaskRunner
};

// Per-output buffer. This can be smaller, but then large sysex messages
// will be (harmlessly) split across multiple seq events. This should
// not have any real practical effect, except perhaps to slightly reorder
// realtime messages with respect to sysex.
constexpr size_t kSendBufferSize = 256;

// Minimum client id for which we will have ALSA card devices for. When we
// are searching for card devices (used to get the path, id, and manufacturer),
// we don't want to get confused by kernel clients that do not have a card.
// See seq_clientmgr.c in the ALSA code for this.
// TODO(agoode): Add proper client -> card export from the kernel to avoid
//               hardcoding.
constexpr int kMinimumClientIdForCards = 16;

// ALSA constants.
const char kAlsaHw[] = "hw";

// udev constants.
const char kUdev[] = "udev";
const char kUdevSubsystemSound[] = "sound";
const char kUdevPropertySoundInitialized[] = "SOUND_INITIALIZED";
const char kUdevActionChange[] = "change";
const char kUdevActionRemove[] = "remove";

const char kUdevIdVendor[] = "ID_VENDOR";
const char kUdevIdVendorEnc[] = "ID_VENDOR_ENC";
const char kUdevIdVendorFromDatabase[] = "ID_VENDOR_FROM_DATABASE";
const char kUdevIdVendorId[] = "ID_VENDOR_ID";
const char kUdevIdModelId[] = "ID_MODEL_ID";
const char kUdevIdBus[] = "ID_BUS";
const char kUdevIdPath[] = "ID_PATH";
const char kUdevIdUsbInterfaceNum[] = "ID_USB_INTERFACE_NUM";
const char kUdevIdSerialShort[] = "ID_SERIAL_SHORT";

const char kSysattrVendorName[] = "vendor_name";
const char kSysattrVendor[] = "vendor";
const char kSysattrModel[] = "model";
const char kSysattrGuid[] = "guid";

const char kCardSyspath[] = "/card";

// Constants for the capabilities we search for in inputs and outputs.
// See http://www.alsa-project.org/alsa-doc/alsa-lib/seq.html.
constexpr unsigned int kRequiredInputPortCaps =
    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
constexpr unsigned int kRequiredOutputPortCaps =
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;

constexpr unsigned int kCreateOutputPortCaps =
    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_NO_EXPORT;
constexpr unsigned int kCreateInputPortCaps =
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_NO_EXPORT;
constexpr unsigned int kCreatePortType =
    SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION;

int AddrToInt(int client, int port) {
  return (client << 8) | port;
}

// Returns true if this client has an ALSA card associated with it.
bool IsCardClient(snd_seq_client_type_t type, int client_id) {
  return (type == SND_SEQ_KERNEL_CLIENT) &&
         (client_id >= kMinimumClientIdForCards);
}

// TODO(agoode): Move this to device/udev_linux.
const std::string UdevDeviceGetPropertyOrSysattr(
    struct udev_device* udev_device,
    const char* property_key,
    const char* sysattr_key) {
  // First try the property.
  std::string value =
      device::UdevDeviceGetPropertyValue(udev_device, property_key);

  // If no property, look for sysattrs and walk up the parent devices too.
  while (value.empty() && udev_device) {
    value = device::UdevDeviceGetSysattrValue(udev_device, sysattr_key);
    udev_device = device::udev_device_get_parent(udev_device);
  }
  return value;
}

int GetCardNumber(udev_device* dev) {
  const char* syspath = device::udev_device_get_syspath(dev);
  if (!syspath)
    return -1;

  std::string syspath_str(syspath);
  size_t i = syspath_str.rfind(kCardSyspath);
  if (i == std::string::npos)
    return -1;

  int number;
  if (!base::StringToInt(syspath_str.substr(i + strlen(kCardSyspath)), &number))
    return -1;
  return number;
}

std::string GetVendor(udev_device* dev) {
  // Try to get the vendor string. Sometimes it is encoded.
  std::string vendor = device::UdevDecodeString(
      device::UdevDeviceGetPropertyValue(dev, kUdevIdVendorEnc));
  // Sometimes it is not encoded.
  if (vendor.empty())
    vendor =
        UdevDeviceGetPropertyOrSysattr(dev, kUdevIdVendor, kSysattrVendorName);
  return vendor;
}

void SetStringIfNonEmpty(base::Value::Dict* value,
                         const std::string& path,
                         const std::string& in_value) {
  if (!in_value.empty())
    value->Set(path, in_value);
}

}  // namespace

MidiManagerAlsa::MidiManagerAlsa(MidiService* service) : MidiManager(service) {}

MidiManagerAlsa::~MidiManagerAlsa() {
  {
    base::AutoLock lock(out_client_lock_);
    // Close the out client. This will trigger the event thread to stop,
    // because of SND_SEQ_EVENT_CLIENT_EXIT.
    out_client_.reset();
  }
  // Ensure that no task is running any more.
  if (!service()->task_service()->UnbindInstance())
    return;

  // |out_client_| should be reset before UnbindInstance() call to avoid
  // a deadlock, but other finalization steps should be implemented after the
  // UnbindInstance() call above, if we need.
}

void MidiManagerAlsa::StartInitialization() {
  if (!service()->task_service()->BindInstance())
    return CompleteInitialization(Result::INITIALIZATION_ERROR);

  // Create client handles and name the clients.
  int err;
  {
    snd_seq_t* in_seq = nullptr;
    err = snd_seq_open(&in_seq, kAlsaHw, SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);
    if (err != 0) {
      VLOG(1) << "snd_seq_open fails: " << snd_strerror(err);
      return CompleteInitialization(Result::INITIALIZATION_ERROR);
    }
    in_client_ = ScopedSndSeqPtr(in_seq);
    in_client_id_ = snd_seq_client_id(in_client_.get());
    err = snd_seq_set_client_name(in_client_.get(), "Chrome (input)");
    if (err != 0) {
      VLOG(1) << "snd_seq_set_client_name fails: " << snd_strerror(err);
      return CompleteInitialization(Result::INITIALIZATION_ERROR);
    }
  }

  {
    snd_seq_t* out_seq = nullptr;
    err = snd_seq_open(&out_seq, kAlsaHw, SND_SEQ_OPEN_OUTPUT, 0);
    if (err != 0) {
      VLOG(1) << "snd_seq_open fails: " << snd_strerror(err);
      return CompleteInitialization(Result::INITIALIZATION_ERROR);
    }
    base::AutoLock lock(out_client_lock_);
    out_client_ = ScopedSndSeqPtr(out_seq);
    out_client_id_ = snd_seq_client_id(out_client_.get());
    err = snd_seq_set_client_name(out_client_.get(), "Chrome (output)");
    if (err != 0) {
      VLOG(1) << "snd_seq_set_client_name fails: " << snd_strerror(err);
      return CompleteInitialization(Result::INITIALIZATION_ERROR);
    }
  }

  // Create input port.
  in_port_id_ = snd_seq_create_simple_port(
      in_client_.get(), NULL, kCreateInputPortCaps, kCreatePortType);
  if (in_port_id_ < 0) {
    VLOG(1) << "snd_seq_create_simple_port fails: "
            << snd_strerror(in_port_id_);
    return CompleteInitialization(Result::INITIALIZATION_ERROR);
  }

  // Subscribe to the announce port.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t announce_sender;
  snd_seq_addr_t announce_dest;
  announce_sender.client = SND_SEQ_CLIENT_SYSTEM;
  announce_sender.port = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
  announce_dest.client = in_client_id_;
  announce_dest.port = in_port_id_;
  snd_seq_port_subscribe_set_sender(subs, &announce_sender);
  snd_seq_port_subscribe_set_dest(subs, &announce_dest);
  err = snd_seq_subscribe_port(in_client_.get(), subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port on the announce port fails: "
            << snd_strerror(err);
    return CompleteInitialization(Result::INITIALIZATION_ERROR);
  }

  // Initialize decoder.
  decoder_ = CreateScopedSndMidiEventPtr(0);
  snd_midi_event_no_status(decoder_.get(), 1);

  // Initialize udev and monitor.
  udev_ = device::ScopedUdevPtr(device::udev_new());
  udev_monitor_ = device::ScopedUdevMonitorPtr(
      device::udev_monitor_new_from_netlink(udev_.get(), kUdev));
  if (!udev_monitor_.get()) {
    VLOG(1) << "udev_monitor_new_from_netlink fails";
    return CompleteInitialization(Result::INITIALIZATION_ERROR);
  }
  err = device::udev_monitor_filter_add_match_subsystem_devtype(
      udev_monitor_.get(), kUdevSubsystemSound, nullptr);
  if (err != 0) {
    VLOG(1) << "udev_monitor_add_match_subsystem fails: "
            << base::safe_strerror(-err);
    return CompleteInitialization(Result::INITIALIZATION_ERROR);
  }
  err = device::udev_monitor_enable_receiving(udev_monitor_.get());
  if (err != 0) {
    VLOG(1) << "udev_monitor_enable_receiving fails: "
            << base::safe_strerror(-err);
    return CompleteInitialization(Result::INITIALIZATION_ERROR);
  }

  // Generate hotplug events for existing ports.
  // TODO(agoode): Check the return value for failure.
  EnumerateAlsaPorts();

  // Generate hotplug events for existing udev devices. This must be done
  // after udev_monitor_enable_receiving() is called. See the algorithm
  // at http://www.signal11.us/oss/udev/.
  EnumerateUdevCards();

  // Start processing events. Don't do this before enumeration of both
  // ALSA and udev.
  service()->task_service()->PostBoundTask(
      kEventTaskRunner,
      base::BindOnce(&MidiManagerAlsa::EventLoop, base::Unretained(this)));

  CompleteInitialization(Result::OK);
}

void MidiManagerAlsa::DispatchSendMidiData(MidiManagerClient* client,
                                           uint32_t port_index,
                                           const std::vector<uint8_t>& data,
                                           base::TimeTicks timestamp) {
  service()->task_service()->PostBoundDelayedTask(
      kSendTaskRunner,
      base::BindOnce(&MidiManagerAlsa::SendMidiData, base::Unretained(this),
                     client, port_index, data),
      MidiService::TimestampToTimeDeltaDelay(timestamp));
}

MidiManagerAlsa::MidiPort::Id::Id() = default;

MidiManagerAlsa::MidiPort::Id::Id(const std::string& bus,
                                  const std::string& vendor_id,
                                  const std::string& model_id,
                                  const std::string& usb_interface_num,
                                  const std::string& serial)
    : bus_(bus),
      vendor_id_(vendor_id),
      model_id_(model_id),
      usb_interface_num_(usb_interface_num),
      serial_(serial) {}

MidiManagerAlsa::MidiPort::Id::Id(const Id&) = default;

MidiManagerAlsa::MidiPort::Id::~Id() = default;

bool MidiManagerAlsa::MidiPort::Id::operator==(const Id& rhs) const {
  return (bus_ == rhs.bus_) && (vendor_id_ == rhs.vendor_id_) &&
         (model_id_ == rhs.model_id_) &&
         (usb_interface_num_ == rhs.usb_interface_num_) &&
         (serial_ == rhs.serial_);
}

bool MidiManagerAlsa::MidiPort::Id::empty() const {
  return bus_.empty() && vendor_id_.empty() && model_id_.empty() &&
         usb_interface_num_.empty() && serial_.empty();
}

MidiManagerAlsa::MidiPort::MidiPort(const std::string& path,
                                    const Id& id,
                                    int client_id,
                                    int port_id,
                                    int midi_device,
                                    const std::string& client_name,
                                    const std::string& port_name,
                                    const std::string& manufacturer,
                                    const std::string& version,
                                    Type type)
    : id_(id),
      midi_device_(midi_device),
      type_(type),
      path_(path),
      client_id_(client_id),
      port_id_(port_id),
      client_name_(client_name),
      port_name_(port_name),
      manufacturer_(manufacturer),
      version_(version) {}

MidiManagerAlsa::MidiPort::~MidiPort() = default;

// Note: keep synchronized with the MidiPort::Match* methods.
std::unique_ptr<base::Value::Dict> MidiManagerAlsa::MidiPort::Value() const {
  std::unique_ptr<base::Value::Dict> value(new base::Value::Dict);

  std::string type;
  switch (type_) {
    case Type::kInput:
      type = "input";
      break;
    case Type::kOutput:
      type = "output";
      break;
  }
  value->Set("type", type);
  SetStringIfNonEmpty(value.get(), "path", path_);
  SetStringIfNonEmpty(value.get(), "clientName", client_name_);
  SetStringIfNonEmpty(value.get(), "portName", port_name_);
  value->Set("clientId", client_id_);
  value->Set("portId", port_id_);
  value->Set("midiDevice", midi_device_);

  // Flatten id fields.
  SetStringIfNonEmpty(value.get(), "bus", id_.bus());
  SetStringIfNonEmpty(value.get(), "vendorId", id_.vendor_id());
  SetStringIfNonEmpty(value.get(), "modelId", id_.model_id());
  SetStringIfNonEmpty(value.get(), "usbInterfaceNum", id_.usb_interface_num());
  SetStringIfNonEmpty(value.get(), "serial", id_.serial());

  return value;
}

std::string MidiManagerAlsa::MidiPort::JSONValue() const {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(*Value().get());
  return json;
}

// TODO(agoode): Do not use SHA256 here. Instead store a persistent
//               mapping and just use a UUID or other random string.
//               http://crbug.com/465320
std::string MidiManagerAlsa::MidiPort::OpaqueKey() const {
  uint8_t hash[crypto::kSHA256Length];
  crypto::SHA256HashString(JSONValue(), hash, sizeof(hash));
  return base::HexEncode(hash);
}

bool MidiManagerAlsa::MidiPort::MatchConnected(const MidiPort& query) const {
  // Matches on:
  // connected == true
  // type
  // path
  // id
  // client_id
  // port_id
  // midi_device
  // client_name
  // port_name
  return connected() && (type() == query.type()) && (path() == query.path()) &&
         (id() == query.id()) && (client_id() == query.client_id()) &&
         (port_id() == query.port_id()) &&
         (midi_device() == query.midi_device()) &&
         (client_name() == query.client_name()) &&
         (port_name() == query.port_name());
}

bool MidiManagerAlsa::MidiPort::MatchCardPass1(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // path
  // id
  // port_id
  // midi_device
  return MatchCardPass2(query) && (path() == query.path());
}

bool MidiManagerAlsa::MidiPort::MatchCardPass2(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // id
  // port_id
  // midi_device
  return !connected() && (type() == query.type()) && (id() == query.id()) &&
         (port_id() == query.port_id()) &&
         (midi_device() == query.midi_device());
}

bool MidiManagerAlsa::MidiPort::MatchNoCardPass1(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // path.empty(), for both this and query
  // id.empty(), for both this and query
  // client_id
  // port_id
  // client_name
  // port_name
  // midi_device == -1, for both this and query
  return MatchNoCardPass2(query) && (client_id() == query.client_id());
}

bool MidiManagerAlsa::MidiPort::MatchNoCardPass2(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // path.empty(), for both this and query
  // id.empty(), for both this and query
  // port_id
  // client_name
  // port_name
  // midi_device == -1, for both this and query
  return !connected() && (type() == query.type()) && path().empty() &&
         query.path().empty() && id().empty() && query.id().empty() &&
         (port_id() == query.port_id()) &&
         (client_name() == query.client_name()) &&
         (port_name() == query.port_name()) && (midi_device() == -1) &&
         (query.midi_device() == -1);
}

MidiManagerAlsa::MidiPortStateBase::~MidiPortStateBase() = default;

MidiManagerAlsa::MidiPortStateBase::iterator
MidiManagerAlsa::MidiPortStateBase::Find(
    const MidiManagerAlsa::MidiPort& port) {
  auto result = FindConnected(port);
  if (result == end())
    result = FindDisconnected(port);
  return result;
}

MidiManagerAlsa::MidiPortStateBase::iterator
MidiManagerAlsa::MidiPortStateBase::FindConnected(
    const MidiManagerAlsa::MidiPort& port) {
  // Exact match required for connected ports.
  return base::ranges::find_if(ports_, [&port](std::unique_ptr<MidiPort>& p) {
    return p->MatchConnected(port);
  });
}

MidiManagerAlsa::MidiPortStateBase::iterator
MidiManagerAlsa::MidiPortStateBase::FindDisconnected(
    const MidiManagerAlsa::MidiPort& port) {
  // Always match on:
  //  type
  // Possible things to match on:
  //  path
  //  id
  //  client_id
  //  port_id
  //  midi_device
  //  client_name
  //  port_name

  if (!port.path().empty()) {
    // If path is present, then we have a card-based client.

    // Pass 1. Match on path, id, midi_device, port_id.
    // This is the best possible match for hardware card-based clients.
    // This will also match the empty id correctly for devices without an id.
    auto it =
        base::ranges::find_if(ports_, [&port](std::unique_ptr<MidiPort>& p) {
          return p->MatchCardPass1(port);
        });
    if (it != ports_.end())
      return it;

    if (!port.id().empty()) {
      // Pass 2. Match on id, midi_device, port_id.
      // This will give us a high-confidence match when a user moves a device to
      // another USB/Firewire/Thunderbolt/etc port, but only works if the device
      // has a hardware id.
      it = base::ranges::find_if(ports_, [&port](std::unique_ptr<MidiPort>& p) {
        return p->MatchCardPass2(port);
      });
      if (it != ports_.end())
        return it;
    }
  } else {
    // Else, we have a non-card-based client.
    // Pass 1. Match on client_id, port_id, client_name, port_name.
    // This will give us a reasonably good match.
    auto it =
        base::ranges::find_if(ports_, [&port](std::unique_ptr<MidiPort>& p) {
          return p->MatchNoCardPass1(port);
        });
    if (it != ports_.end())
      return it;

    // Pass 2. Match on port_id, client_name, port_name.
    // This is weaker but similar to pass 2 in the hardware card-based clients
    // match.
    it = base::ranges::find_if(ports_, [&port](std::unique_ptr<MidiPort>& p) {
      return p->MatchNoCardPass2(port);
    });
    if (it != ports_.end())
      return it;
  }

  // No match.
  return ports_.end();
}

MidiManagerAlsa::MidiPortStateBase::MidiPortStateBase() = default;

MidiManagerAlsa::MidiPortState::MidiPortState() = default;

uint32_t MidiManagerAlsa::MidiPortState::push_back(
    std::unique_ptr<MidiPort> port) {
  // Add the web midi index.
  uint32_t web_port_index = 0;
  switch (port->type()) {
    case MidiPort::Type::kInput:
      web_port_index = num_input_ports_++;
      break;
    case MidiPort::Type::kOutput:
      web_port_index = num_output_ports_++;
      break;
  }
  port->set_web_port_index(web_port_index);
  MidiPortStateBase::push_back(std::move(port));
  return web_port_index;
}

MidiManagerAlsa::AlsaSeqState::AlsaSeqState() = default;

MidiManagerAlsa::AlsaSeqState::~AlsaSeqState() = default;

void MidiManagerAlsa::AlsaSeqState::ClientStart(int client_id,
                                                const std::string& client_name,
                                                snd_seq_client_type_t type) {
  ClientExit(client_id);
  clients_.insert(
      std::make_pair(client_id, std::make_unique<Client>(client_name, type)));
  if (IsCardClient(type, client_id))
    ++card_client_count_;
}

bool MidiManagerAlsa::AlsaSeqState::ClientStarted(int client_id) {
  return clients_.find(client_id) != clients_.end();
}

void MidiManagerAlsa::AlsaSeqState::ClientExit(int client_id) {
  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    if (IsCardClient(it->second->type(), client_id))
      --card_client_count_;
    clients_.erase(it);
  }
}

void MidiManagerAlsa::AlsaSeqState::PortStart(
    int client_id,
    int port_id,
    const std::string& port_name,
    MidiManagerAlsa::AlsaSeqState::PortDirection direction,
    bool midi) {
  auto it = clients_.find(client_id);
  if (it != clients_.end())
    it->second->AddPort(port_id,
                        std::make_unique<Port>(port_name, direction, midi));
}

void MidiManagerAlsa::AlsaSeqState::PortExit(int client_id, int port_id) {
  auto it = clients_.find(client_id);
  if (it != clients_.end())
    it->second->RemovePort(port_id);
}

snd_seq_client_type_t MidiManagerAlsa::AlsaSeqState::ClientType(
    int client_id) const {
  auto it = clients_.find(client_id);
  if (it == clients_.end())
    return SND_SEQ_USER_CLIENT;
  return it->second->type();
}

std::unique_ptr<MidiManagerAlsa::TemporaryMidiPortState>
MidiManagerAlsa::AlsaSeqState::ToMidiPortState(const AlsaCardMap& alsa_cards) {
  std::unique_ptr<MidiManagerAlsa::TemporaryMidiPortState> midi_ports(
      new TemporaryMidiPortState);
  auto card_it = alsa_cards.begin();

  int card_midi_device = -1;
  for (const auto& client_pair : clients_) {
    int client_id = client_pair.first;
    auto* client = client_pair.second.get();

    // Get client metadata.
    const std::string client_name = client->name();
    std::string manufacturer;
    std::string driver;
    std::string path;
    MidiPort::Id id;
    std::string card_name;
    std::string card_longname;
    int midi_device = -1;

    if (IsCardClient(client->type(), client_id)) {
      auto& card = card_it->second;
      if (card_midi_device == -1)
        card_midi_device = 0;

      manufacturer = card->manufacturer();
      path = card->path();
      id = MidiPort::Id(card->bus(), card->vendor_id(), card->model_id(),
                        card->usb_interface_num(), card->serial());
      card_name = card->name();
      card_longname = card->longname();
      midi_device = card_midi_device;

      ++card_midi_device;
      if (card_midi_device >= card->midi_device_count()) {
        card_midi_device = -1;
        ++card_it;
      }
    }

    for (const auto& port_pair : *client) {
      int port_id = port_pair.first;
      const auto& port = port_pair.second;

      if (port->midi()) {
        std::string version;
        if (!driver.empty()) {
          version = driver + " / ";
        }
        version +=
            base::StringPrintf("ALSA library version %d.%d.%d", SND_LIB_MAJOR,
                               SND_LIB_MINOR, SND_LIB_SUBMINOR);
        PortDirection direction = port->direction();
        if (direction == PortDirection::kInput ||
            direction == PortDirection::kDuplex) {
          midi_ports->push_back(std::make_unique<MidiPort>(
              path, id, client_id, port_id, midi_device, client->name(),
              port->name(), manufacturer, version, MidiPort::Type::kInput));
        }
        if (direction == PortDirection::kOutput ||
            direction == PortDirection::kDuplex) {
          midi_ports->push_back(std::make_unique<MidiPort>(
              path, id, client_id, port_id, midi_device, client->name(),
              port->name(), manufacturer, version, MidiPort::Type::kOutput));
        }
      }
    }
  }

  return midi_ports;
}

MidiManagerAlsa::AlsaSeqState::Port::Port(
    const std::string& name,
    MidiManagerAlsa::AlsaSeqState::PortDirection direction,
    bool midi)
    : name_(name), direction_(direction), midi_(midi) {}

MidiManagerAlsa::AlsaSeqState::Port::~Port() = default;

MidiManagerAlsa::AlsaSeqState::Client::Client(const std::string& name,
                                              snd_seq_client_type_t type)
    : name_(name), type_(type) {}

MidiManagerAlsa::AlsaSeqState::Client::~Client() = default;

void MidiManagerAlsa::AlsaSeqState::Client::AddPort(
    int addr,
    std::unique_ptr<Port> port) {
  ports_[addr] = std::move(port);
}

void MidiManagerAlsa::AlsaSeqState::Client::RemovePort(int addr) {
  ports_.erase(addr);
}

MidiManagerAlsa::AlsaSeqState::Client::PortMap::const_iterator
MidiManagerAlsa::AlsaSeqState::Client::begin() const {
  return ports_.begin();
}

MidiManagerAlsa::AlsaSeqState::Client::PortMap::const_iterator
MidiManagerAlsa::AlsaSeqState::Client::end() const {
  return ports_.end();
}

MidiManagerAlsa::AlsaCard::AlsaCard(udev_device* dev,
                                    const std::string& name,
                                    const std::string& longname,
                                    const std::string& driver,
                                    int midi_device_count)
    : name_(name),
      longname_(longname),
      driver_(driver),
      path_(device::UdevDeviceGetPropertyValue(dev, kUdevIdPath)),
      bus_(device::UdevDeviceGetPropertyValue(dev, kUdevIdBus)),
      vendor_id_(
          UdevDeviceGetPropertyOrSysattr(dev, kUdevIdVendorId, kSysattrVendor)),
      model_id_(
          UdevDeviceGetPropertyOrSysattr(dev, kUdevIdModelId, kSysattrModel)),
      usb_interface_num_(
          device::UdevDeviceGetPropertyValue(dev, kUdevIdUsbInterfaceNum)),
      serial_(UdevDeviceGetPropertyOrSysattr(dev,
                                             kUdevIdSerialShort,
                                             kSysattrGuid)),
      midi_device_count_(midi_device_count),
      manufacturer_(ExtractManufacturerString(
          GetVendor(dev),
          vendor_id_,
          device::UdevDeviceGetPropertyValue(dev, kUdevIdVendorFromDatabase),
          name,
          longname)) {}

MidiManagerAlsa::AlsaCard::~AlsaCard() = default;

// static
std::string MidiManagerAlsa::AlsaCard::ExtractManufacturerString(
    const std::string& udev_id_vendor,
    const std::string& udev_id_vendor_id,
    const std::string& udev_id_vendor_from_database,
    const std::string& alsa_name,
    const std::string& alsa_longname) {
  // Let's try to determine the manufacturer. Here is the ordered preference
  // in extraction:
  //  1. Vendor name from the hardware device string, from udev properties
  //     or sysattrs.
  //  2. Vendor name from the udev database (property ID_VENDOR_FROM_DATABASE).
  //  3. Heuristic from ALSA.

  // Is the vendor string present and not just the vendor hex id?
  if (!udev_id_vendor.empty() && (udev_id_vendor != udev_id_vendor_id)) {
    return udev_id_vendor;
  }

  // Is there a vendor string in the hardware database?
  if (!udev_id_vendor_from_database.empty()) {
    return udev_id_vendor_from_database;
  }

  // Ok, udev gave us nothing useful, or was unavailable. So try a heuristic.
  // We assume that card longname is in the format of
  // "<manufacturer> <name> at <bus>". Otherwise, we give up to detect
  // a manufacturer name here.
  size_t at_index = alsa_longname.rfind(" at ");
  if (at_index && at_index != std::string::npos) {
    size_t name_index = alsa_longname.rfind(alsa_name, at_index - 1);
    if (name_index && name_index != std::string::npos)
      return alsa_longname.substr(0, name_index - 1);
  }

  // Failure.
  return "";
}

void MidiManagerAlsa::SendMidiData(MidiManagerClient* client,
                                   uint32_t port_index,
                                   const std::vector<uint8_t>& data) {
  ScopedSndMidiEventPtr encoder = CreateScopedSndMidiEventPtr(kSendBufferSize);
  for (const auto datum : data) {
    snd_seq_event_t event;
    snd_seq_ev_clear(&event);
    int result = snd_midi_event_encode_byte(encoder.get(), datum, &event);
    if (result == 1) {
      // Full event, send it.
      base::AutoLock ports_lock(out_ports_lock_);
      auto it = out_ports_.find(port_index);
      if (it != out_ports_.end()) {
        base::AutoLock client_lock(out_client_lock_);
        if (!out_client_)
          return;
        snd_seq_ev_set_source(&event, it->second);
        snd_seq_ev_set_subs(&event);
        snd_seq_ev_set_direct(&event);
        snd_seq_event_output_direct(out_client_.get(), &event);
      }
    }
  }

  // Acknowledge send.
  AccumulateMidiBytesSent(client, data.size());
}

void MidiManagerAlsa::EventLoop() {
  bool loop_again = true;

  struct pollfd pfd[2];
  snd_seq_poll_descriptors(in_client_.get(), &pfd[0], 1, POLLIN);
  pfd[1].fd = device::udev_monitor_get_fd(udev_monitor_.get());
  pfd[1].events = POLLIN;

  int err = HANDLE_EINTR(poll(pfd, std::size(pfd), -1));
  if (err < 0) {
    VPLOG(1) << "poll failed";
    loop_again = false;
  } else {
    if (pfd[0].revents & POLLIN) {
      // Read available incoming MIDI data.
      int remaining;
      base::TimeTicks timestamp = base::TimeTicks::Now();
      do {
        snd_seq_event_t* event;
        err = snd_seq_event_input(in_client_.get(), &event);
        remaining = snd_seq_event_input_pending(in_client_.get(), 0);

        if (err == -ENOSPC) {
          // Handle out of space error.
          VLOG(1) << "snd_seq_event_input detected buffer overrun";
          // We've lost events: check another way to see if we need to shut
          // down.
        } else if (err == -EAGAIN) {
          // We've read all the data.
        } else if (err < 0) {
          // Handle other errors.
          VLOG(1) << "snd_seq_event_input fails: " << snd_strerror(err);
          // TODO(agoode): Use RecordAction() or similar to log this.
          loop_again = false;
        } else if (event->source.client == SND_SEQ_CLIENT_SYSTEM &&
                   event->source.port == SND_SEQ_PORT_SYSTEM_ANNOUNCE) {
          // Handle announce events.
          switch (event->type) {
            case SND_SEQ_EVENT_PORT_START:
              // Don't use SND_SEQ_EVENT_CLIENT_START because the
              // client name may not be set by the time we query
              // it. It should be set by the time ports are made.
              ProcessClientStartEvent(event->data.addr.client);
              ProcessPortStartEvent(event->data.addr);
              break;
            case SND_SEQ_EVENT_CLIENT_EXIT:
              // Check for disconnection of our "out" client. This means "shut
              // down".
              if (event->data.addr.client == out_client_id_) {
                loop_again = false;
                remaining = 0;
              } else
                ProcessClientExitEvent(event->data.addr);
              break;
            case SND_SEQ_EVENT_PORT_EXIT:
              ProcessPortExitEvent(event->data.addr);
              break;
          }
        } else {
          // Normal operation.
          ProcessSingleEvent(event, timestamp);
        }
      } while (remaining > 0);
    }
    if (pfd[1].revents & POLLIN) {
      device::ScopedUdevDevicePtr dev(
          device::udev_monitor_receive_device(udev_monitor_.get()));
      if (dev.get())
        ProcessUdevEvent(dev.get());
      else
        VLOG(1) << "udev_monitor_receive_device fails";
    }
  }

  // Do again.
  if (loop_again) {
    service()->task_service()->PostBoundTask(
        kEventTaskRunner,
        base::BindOnce(&MidiManagerAlsa::EventLoop, base::Unretained(this)));
  }
}

void MidiManagerAlsa::ProcessSingleEvent(snd_seq_event_t* event,
                                         base::TimeTicks timestamp) {
  auto source_it =
      source_map_.find(AddrToInt(event->source.client, event->source.port));
  if (source_it != source_map_.end()) {
    uint32_t source = source_it->second;
    if (event->type == SND_SEQ_EVENT_SYSEX) {
      // Special! Variable-length sysex.
      ReceiveMidiData(source, static_cast<const uint8_t*>(event->data.ext.ptr),
                      event->data.ext.len, timestamp);
    } else {
      // Otherwise, decode this and send that on.
      unsigned char buf[12];
      long count =
          snd_midi_event_decode(decoder_.get(), buf, sizeof(buf), event);
      if (count <= 0) {
        if (count != -ENOENT) {
          // ENOENT means that it's not a MIDI message, which is not an
          // error, but other negative values are errors for us.
          VLOG(1) << "snd_midi_event_decoder fails " << snd_strerror(count);
          // TODO(agoode): Record this failure.
        }
      } else {
        ReceiveMidiData(source, buf, count, timestamp);
      }
    }
  }
}

void MidiManagerAlsa::ProcessClientStartEvent(int client_id) {
  // Ignore if client is already started.
  if (alsa_seq_state_.ClientStarted(client_id))
    return;

  snd_seq_client_info_t* client_info;
  snd_seq_client_info_alloca(&client_info);
  int err =
      snd_seq_get_any_client_info(in_client_.get(), client_id, client_info);
  if (err != 0)
    return;

  // Skip our own clients.
  if ((client_id == in_client_id_) || (client_id == out_client_id_))
    return;

  // Update our view of ALSA seq state.
  alsa_seq_state_.ClientStart(client_id,
                              snd_seq_client_info_get_name(client_info),
                              snd_seq_client_info_get_type(client_info));

  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessPortStartEvent(const snd_seq_addr_t& addr) {
  snd_seq_port_info_t* port_info;
  snd_seq_port_info_alloca(&port_info);
  int err = snd_seq_get_any_port_info(in_client_.get(), addr.client, addr.port,
                                      port_info);
  if (err != 0)
    return;

  unsigned int caps = snd_seq_port_info_get_capability(port_info);
  bool input = (caps & kRequiredInputPortCaps) == kRequiredInputPortCaps;
  bool output = (caps & kRequiredOutputPortCaps) == kRequiredOutputPortCaps;
  AlsaSeqState::PortDirection direction;
  if (input && output)
    direction = AlsaSeqState::PortDirection::kDuplex;
  else if (input)
    direction = AlsaSeqState::PortDirection::kInput;
  else if (output)
    direction = AlsaSeqState::PortDirection::kOutput;
  else
    return;

  // Update our view of ALSA seq state.
  alsa_seq_state_.PortStart(
      addr.client, addr.port, snd_seq_port_info_get_name(port_info), direction,
      snd_seq_port_info_get_type(port_info) & SND_SEQ_PORT_TYPE_MIDI_GENERIC);
  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessClientExitEvent(const snd_seq_addr_t& addr) {
  // Update our view of ALSA seq state.
  alsa_seq_state_.ClientExit(addr.client);
  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessPortExitEvent(const snd_seq_addr_t& addr) {
  // Update our view of ALSA seq state.
  alsa_seq_state_.PortExit(addr.client, addr.port);
  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessUdevEvent(udev_device* dev) {
  // Only card devices have this property set, and only when they are
  // fully initialized.
  if (!device::udev_device_get_property_value(dev,
                                              kUdevPropertySoundInitialized))
    return;

  // Get the action. If no action, then we are doing first time enumeration
  // and the device is treated as new.
  const char* action = device::udev_device_get_action(dev);
  if (!action)
    action = kUdevActionChange;

  if (strcmp(action, kUdevActionChange) == 0) {
    AddCard(dev);
    // Generate Web MIDI events.
    UpdatePortStateAndGenerateEvents();
  } else if (strcmp(action, kUdevActionRemove) == 0) {
    RemoveCard(GetCardNumber(dev));
    // Generate Web MIDI events.
    UpdatePortStateAndGenerateEvents();
  }
}

void MidiManagerAlsa::AddCard(udev_device* dev) {
  int number = GetCardNumber(dev);
  if (number == -1)
    return;

  RemoveCard(number);

  snd_ctl_card_info_t* card;
  snd_hwdep_info_t* hwdep;
  snd_ctl_card_info_alloca(&card);
  snd_hwdep_info_alloca(&hwdep);
  const std::string id = base::StringPrintf("hw:CARD=%i", number);
  snd_ctl_t* handle;
  int err = snd_ctl_open(&handle, id.c_str(), 0);
  if (err != 0) {
    VLOG(1) << "snd_ctl_open fails: " << snd_strerror(err);
    return;
  }
  err = snd_ctl_card_info(handle, card);
  if (err != 0) {
    VLOG(1) << "snd_ctl_card_info fails: " << snd_strerror(err);
    snd_ctl_close(handle);
    return;
  }
  std::string name = snd_ctl_card_info_get_name(card);
  std::string longname = snd_ctl_card_info_get_longname(card);
  std::string driver = snd_ctl_card_info_get_driver(card);

  // Count rawmidi devices (not subdevices).
  int midi_count = 0;
  for (int device = -1;
       !snd_ctl_rawmidi_next_device(handle, &device) && device >= 0;)
    ++midi_count;

  // Count any hwdep synths that become MIDI devices outside of rawmidi.
  //
  // Explanation:
  // Any kernel driver can create an ALSA client (visible to us).
  // With modern hardware, only rawmidi devices do this. Kernel
  // drivers create rawmidi devices and the rawmidi subsystem makes
  // the seq clients. But the OPL3 driver is special, it does not
  // make a rawmidi device but a seq client directly. (This is the
  // only one to worry about in the kernel code, as of 2015-03-23.)
  //
  // OPL3 is very old (but still possible to get in new
  // hardware). It is unlikely that new drivers would not use
  // rawmidi and defeat our heuristic.
  //
  // Longer term, support should be added in the kernel to expose a
  // direct link from card->client (or client->card) so that all
  // these heuristics will be obsolete.  Once that is there, we can
  // assume our old heuristics will work on old kernels and the new
  // robust code will be used on new. Then we will not need to worry
  // about changes to kernel internals breaking our code.
  // See the TODO above at kMinimumClientIdForCards.
  for (int device = -1;
       !snd_ctl_hwdep_next_device(handle, &device) && device >= 0;) {
    err = snd_ctl_hwdep_info(handle, hwdep);
    if (err != 0) {
      VLOG(1) << "snd_ctl_hwdep_info fails: " << snd_strerror(err);
      continue;
    }
    snd_hwdep_iface_t iface = snd_hwdep_info_get_iface(hwdep);
    if (iface == SND_HWDEP_IFACE_OPL2 || iface == SND_HWDEP_IFACE_OPL3 ||
        iface == SND_HWDEP_IFACE_OPL4)
      ++midi_count;
  }
  snd_ctl_close(handle);

  if (midi_count > 0) {
    auto alsa_card =
        std::make_unique<AlsaCard>(dev, name, longname, driver, midi_count);
    alsa_cards_.insert(std::make_pair(number, std::move(alsa_card)));
    alsa_card_midi_count_ += midi_count;
  }
}

void MidiManagerAlsa::RemoveCard(int number) {
  auto it = alsa_cards_.find(number);
  if (it == alsa_cards_.end())
    return;

  alsa_card_midi_count_ -= it->second->midi_device_count();
  alsa_cards_.erase(it);
}

void MidiManagerAlsa::UpdatePortStateAndGenerateEvents() {
  // Verify that our information from ALSA and udev are in sync. If
  // not, we cannot generate events right now.
  if (alsa_card_midi_count_ != alsa_seq_state_.card_client_count())
    return;

  // Generate new port state.
  auto new_port_state = alsa_seq_state_.ToMidiPortState(alsa_cards_);

  // Disconnect any connected old ports that are now missing.
  for (auto& old_port : port_state_) {
    if (old_port->connected() &&
        (new_port_state->FindConnected(*old_port) == new_port_state->end())) {
      old_port->set_connected(false);
      uint32_t web_port_index = old_port->web_port_index();
      switch (old_port->type()) {
        case MidiPort::Type::kInput:
          source_map_.erase(
              AddrToInt(old_port->client_id(), old_port->port_id()));
          SetInputPortState(web_port_index, PortState::DISCONNECTED);
          break;
        case MidiPort::Type::kOutput:
          DeleteAlsaOutputPort(web_port_index);
          SetOutputPortState(web_port_index, PortState::DISCONNECTED);
          break;
      }
    }
  }

  // Reconnect or add new ports.
  auto it = new_port_state->begin();
  while (it != new_port_state->end()) {
    auto& new_port = *it;
    auto old_port = port_state_.Find(*new_port);
    if (old_port == port_state_.end()) {
      // Add new port.
      const auto& opaque_key = new_port->OpaqueKey();
      const auto& manufacturer = new_port->manufacturer();
      const auto& port_name = new_port->port_name();
      const auto& version = new_port->version();
      const auto& type = new_port->type();
      const auto& client_id = new_port->client_id();
      const auto& port_id = new_port->port_id();

      uint32_t web_port_index = port_state_.push_back(std::move(new_port));
      it = new_port_state->erase(it);

      mojom::PortInfo info(opaque_key, manufacturer, port_name, version,
                           PortState::OPENED);
      switch (type) {
        case MidiPort::Type::kInput:
          if (Subscribe(web_port_index, client_id, port_id))
            AddInputPort(info);
          break;
        case MidiPort::Type::kOutput:
          if (CreateAlsaOutputPort(web_port_index, client_id, port_id))
            AddOutputPort(info);
          break;
      }
    } else if (!(*old_port)->connected()) {
      // Reconnect.
      uint32_t web_port_index = (*old_port)->web_port_index();
      (*old_port)->Update(new_port->path(), new_port->client_id(),
                          new_port->port_id(), new_port->client_name(),
                          new_port->port_name(), new_port->manufacturer(),
                          new_port->version());
      switch ((*old_port)->type()) {
        case MidiPort::Type::kInput:
          if (Subscribe(web_port_index, (*old_port)->client_id(),
                        (*old_port)->port_id()))
            SetInputPortState(web_port_index, PortState::OPENED);
          break;
        case MidiPort::Type::kOutput:
          if (CreateAlsaOutputPort(web_port_index, (*old_port)->client_id(),
                                   (*old_port)->port_id()))
            SetOutputPortState(web_port_index, PortState::OPENED);
          break;
      }
      (*old_port)->set_connected(true);
      ++it;
    } else {
      ++it;
    }
  }
}

// TODO(agoode): return false on failure.
void MidiManagerAlsa::EnumerateAlsaPorts() {
  snd_seq_client_info_t* client_info;
  snd_seq_client_info_alloca(&client_info);
  snd_seq_port_info_t* port_info;
  snd_seq_port_info_alloca(&port_info);

  // Enumerate clients.
  snd_seq_client_info_set_client(client_info, -1);
  while (!snd_seq_query_next_client(in_client_.get(), client_info)) {
    int client_id = snd_seq_client_info_get_client(client_info);
    ProcessClientStartEvent(client_id);

    // Enumerate ports.
    snd_seq_port_info_set_client(port_info, client_id);
    snd_seq_port_info_set_port(port_info, -1);
    while (!snd_seq_query_next_port(in_client_.get(), port_info)) {
      const snd_seq_addr_t* addr = snd_seq_port_info_get_addr(port_info);
      ProcessPortStartEvent(*addr);
    }
  }
}

bool MidiManagerAlsa::EnumerateUdevCards() {
  int err;

  device::ScopedUdevEnumeratePtr enumerate(
      device::udev_enumerate_new(udev_.get()));
  if (!enumerate.get()) {
    VLOG(1) << "udev_enumerate_new fails";
    return false;
  }

  err = device::udev_enumerate_add_match_subsystem(enumerate.get(),
                                                   kUdevSubsystemSound);
  if (err) {
    VLOG(1) << "udev_enumerate_add_match_subsystem fails: "
            << base::safe_strerror(-err);
    return false;
  }

  err = device::udev_enumerate_scan_devices(enumerate.get());
  if (err) {
    VLOG(1) << "udev_enumerate_scan_devices fails: "
            << base::safe_strerror(-err);
    return false;
  }

  udev_list_entry* list_entry;
  auto* devices = device::udev_enumerate_get_list_entry(enumerate.get());
  udev_list_entry_foreach(list_entry, devices) {
    const char* path = device::udev_list_entry_get_name(list_entry);
    device::ScopedUdevDevicePtr dev(
        device::udev_device_new_from_syspath(udev_.get(), path));
    if (dev.get())
      ProcessUdevEvent(dev.get());
  }

  return true;
}

bool MidiManagerAlsa::CreateAlsaOutputPort(uint32_t port_index,
                                           int client_id,
                                           int port_id) {
  // Create the port.
  int out_port;
  {
    base::AutoLock lock(out_client_lock_);
    out_port = snd_seq_create_simple_port(
        out_client_.get(), NULL, kCreateOutputPortCaps, kCreatePortType);

    if (out_port < 0) {
      VLOG(1) << "snd_seq_create_simple_port fails: " << snd_strerror(out_port);
      return false;
    }

    // Activate port subscription.
    snd_seq_port_subscribe_t* subs;
    snd_seq_port_subscribe_alloca(&subs);
    snd_seq_addr_t sender;
    sender.client = out_client_id_;
    sender.port = out_port;
    snd_seq_port_subscribe_set_sender(subs, &sender);
    snd_seq_addr_t dest;
    dest.client = client_id;
    dest.port = port_id;
    snd_seq_port_subscribe_set_dest(subs, &dest);
    int err = snd_seq_subscribe_port(out_client_.get(), subs);
    if (err != 0) {
      VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
      snd_seq_delete_simple_port(out_client_.get(), out_port);
      return false;
    }
  }

  // Update our map.
  base::AutoLock lock(out_ports_lock_);
  out_ports_[port_index] = out_port;
  return true;
}

void MidiManagerAlsa::DeleteAlsaOutputPort(uint32_t port_index) {
  int alsa_port;
  {
    base::AutoLock lock(out_ports_lock_);
    auto it = out_ports_.find(port_index);
    if (it == out_ports_.end())
      return;
    alsa_port = it->second;
    out_ports_.erase(it);
  }
  {
    base::AutoLock lock(out_client_lock_);
    snd_seq_delete_simple_port(out_client_.get(), alsa_port);
  }
}

bool MidiManagerAlsa::Subscribe(uint32_t port_index,
                                int client_id,
                                int port_id) {
  // Activate port subscription.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t sender;
  sender.client = client_id;
  sender.port = port_id;
  snd_seq_port_subscribe_set_sender(subs, &sender);
  snd_seq_addr_t dest;
  dest.client = in_client_id_;
  dest.port = in_port_id_;
  snd_seq_port_subscribe_set_dest(subs, &dest);
  int err = snd_seq_subscribe_port(in_client_.get(), subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
    return false;
  }

  // Update our map.
  source_map_[AddrToInt(client_id, port_id)] = port_index;
  return true;
}

MidiManagerAlsa::ScopedSndMidiEventPtr
MidiManagerAlsa::CreateScopedSndMidiEventPtr(size_t size) {
  snd_midi_event_t* coder;
  snd_midi_event_new(size, &coder);
  return ScopedSndMidiEventPtr(coder);
}

MidiManager* MidiManager::Create(MidiService* service) {
  return new MidiManagerAlsa(service);
}

}  // namespace midi
