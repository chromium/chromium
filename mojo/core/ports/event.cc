// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/event.h"

#include <stdint.h>
#include <string.h>

#include "base/numerics/safe_math.h"
#include "mojo/core/ports/user_message.h"

namespace mojo {
namespace core {
namespace ports {

namespace {

const size_t kPortsMessageAlignment = 8;

#pragma pack(push, 1)

struct SerializedHeader {
  Event::Type type;
  uint32_t padding;
  PortName port_name;
};

struct UserMessageEventData {
  uint64_t sequence_num;
  uint32_t num_ports;
  uint32_t padding;
};

struct ObserveProxyEventData {
  NodeName proxy_node_name;
  PortName proxy_port_name;
  NodeName proxy_target_node_name;
  PortName proxy_target_port_name;
};

struct ObserveProxyAckEventData {
  uint64_t last_sequence_num;
};

struct ObserveClosureEventData {
  uint64_t last_sequence_num;
};

struct MergePortEventData {
  PortName new_port_name;
  Event::PortDescriptor new_port_descriptor;
};

struct UserMessageReadAckRequestEventData {
  uint64_t sequence_num_to_acknowledge;
};

struct UserMessageReadAckEventData {
  uint64_t sequence_num_acknowledged;
};

#pragma pack(pop)

static_assert(sizeof(Event::PortDescriptor) % kPortsMessageAlignment == 0,
              "Invalid PortDescriptor size.");

static_assert(sizeof(SerializedHeader) % kPortsMessageAlignment == 0,
              "Invalid SerializedHeader size.");

static_assert(sizeof(UserMessageEventData) % kPortsMessageAlignment == 0,
              "Invalid UserEventData size.");

static_assert(sizeof(ObserveProxyEventData) % kPortsMessageAlignment == 0,
              "Invalid ObserveProxyEventData size.");

static_assert(sizeof(ObserveProxyAckEventData) % kPortsMessageAlignment == 0,
              "Invalid ObserveProxyAckEventData size.");

static_assert(sizeof(ObserveClosureEventData) % kPortsMessageAlignment == 0,
              "Invalid ObserveClosureEventData size.");

static_assert(sizeof(MergePortEventData) % kPortsMessageAlignment == 0,
              "Invalid MergePortEventData size.");

static_assert(sizeof(UserMessageReadAckRequestEventData) %
                      kPortsMessageAlignment ==
                  0,
              "Invalid UserMessageReadAckRequestEventData size.");

static_assert(sizeof(UserMessageReadAckEventData) % kPortsMessageAlignment == 0,
              "Invalid UserMessageReadAckEventData size.");

}  // namespace

Event::PortDescriptor::PortDescriptor() {
  memset(padding, 0, sizeof(padding));
}

Event::~Event() = default;

// static
ScopedEvent Event::Deserialize(const void* buffer, size_t num_bytes) {
  if (num_bytes < sizeof(SerializedHeader))
    return nullptr;

  const auto* header = static_cast<const SerializedHeader*>(buffer);
  const PortName& port_name = header->port_name;
  const size_t data_size = num_bytes - sizeof(*header);
  switch (header->type) {
    case Type::kUserMessage:
      return UserMessageEvent::Deserialize(port_name, header + 1, data_size);
    case Type::kPortAccepted:
      return PortAcceptedEvent::Deserialize(port_name, header + 1, data_size);
    case Type::kObserveProxy:
      return ObserveProxyEvent::Deserialize(port_name, header + 1, data_size);
    case Type::kObserveProxyAck:
      return ObserveProxyAckEvent::Deserialize(port_name, header + 1,
                                               data_size);
    case Type::kObserveClosure:
      return ObserveClosureEvent::Deserialize(port_name, header + 1, data_size);
    case Type::kMergePort:
      return MergePortEvent::Deserialize(port_name, header + 1, data_size);
    default:
      DVLOG(2) << "Ingoring unknown port event type: "
               << static_cast<uint32_t>(header->type);
      return nullptr;
  }
}

Event::Event(Type type, const PortName& port_name)
    : type_(type), port_name_(port_name) {}

size_t Event::GetSerializedSize() const {
  return sizeof(SerializedHeader) + GetSerializedDataSize();
}

void Event::Serialize(void* buffer) const {
  auto* header = static_cast<SerializedHeader*>(buffer);
  header->type = type_;
  header->padding = 0;
  header->port_name = port_name_;
  SerializeData(header + 1);
}

ScopedEvent Event::Clone() const {
  return nullptr;
}

UserMessageEvent::~UserMessageEvent() = default;

UserMessageEvent::UserMessageEvent(size_t num_ports)
    : Event(Type::kUserMessage, kInvalidPortName) {
  ReservePorts(num_ports);
}

void UserMessageEvent::AttachMessage(std::unique_ptr<UserMessage> message) {
  DCHECK(!message_);
  message_ = std::move(message);
}

void UserMessageEvent::ReservePorts(size_t num_ports) {
  port_descriptors_.resize(num_ports);
  ports_.resize(num_ports);
}

bool UserMessageEvent::NotifyWillBeRoutedExternally() {
  DCHECK(message_);
  return message_->WillBeRoutedExternally();
}

// static
ScopedEvent UserMessageEvent::Deserialize(const PortName& port_name,
                                          const void* buffer,
                                          size_t num_bytes) {
  if (num_bytes < sizeof(UserMessageEventData))
    return nullptr;

  const auto* data = static_cast<const UserMessageEventData*>(buffer);
  base::CheckedNumeric<size_t> port_data_size = data->num_ports;
  port_data_size *= sizeof(PortDescriptor) + sizeof(PortName);
  if (!port_data_size.IsValid())
    return nullptr;

  base::CheckedNumeric<size_t> total_size = port_data_size.ValueOrDie();
  total_size += sizeof(UserMessageEventData);
  if (!total_size.IsValid() || num_bytes < total_size.ValueOrDie())
    return nullptr;

  auto event =
      base::WrapUnique(new UserMessageEvent(port_name, data->sequence_num));
  event->ReservePorts(data->num_ports);
  const auto* in_descriptors =
      reinterpret_cast<const PortDescriptor*>(data + 1);
  std::copy(in_descriptors, in_descriptors + data->num_ports,
            event->port_descriptors());

  const auto* in_names =
      reinterpret_cast<const PortName*>(in_descriptors + data->num_ports);
  std::copy(in_names, in_names + data->num_ports, event->ports());
  return std::move(event);
}

UserMessageEvent::UserMessageEvent(const PortName& port_name,
                                   uint64_t sequence_num)
    : Event(Type::kUserMessage, port_name), sequence_num_(sequence_num) {}

size_t UserMessageEvent::GetSizeIfSerialized() const {
  if (!message_)
    return 0;
  return message_->GetSizeIfSerialized();
}

size_t UserMessageEvent::GetSerializedDataSize() const {
  DCHECK_EQ(ports_.size(), port_descriptors_.size());
  base::CheckedNumeric<size_t> size = sizeof(UserMessageEventData);
  base::CheckedNumeric<size_t> ports_size =
      sizeof(PortDescriptor) + sizeof(PortName);
  ports_size *= ports_.size();
  return (size + ports_size.ValueOrDie()).ValueOrDie();
}

void UserMessageEvent::SerializeData(void* buffer) const {
  DCHECK_EQ(ports_.size(), port_descriptors_.size());
  auto* data = static_cast<UserMessageEventData*>(buffer);
  data->sequence_num = sequence_num_;
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(ports_.size()));
  data->num_ports = static_cast<uint32_t>(ports_.size());
  data->padding = 0;

  auto* ports_data = reinterpret_cast<PortDescriptor*>(data + 1);
  std::copy(port_descriptors_.begin(), port_descriptors_.end(), ports_data);

  auto* port_names_data =
      reinterpret_cast<PortName*>(ports_data + ports_.size());
  std::copy(ports_.begin(), ports_.end(), port_names_data);
}

PortAcceptedEvent::PortAcceptedEvent(const PortName& port_name)
    : Event(Type::kPortAccepted, port_name) {}

PortAcceptedEvent::~PortAcceptedEvent() = default;

// static
ScopedEvent PortAcceptedEvent::Deserialize(const PortName& port_name,
                                           const void* buffer,
                                           size_t num_bytes) {
  return std::make_unique<PortAcceptedEvent>(port_name);
}

size_t PortAcceptedEvent::GetSerializedDataSize() const {
  return 0;
}

void PortAcceptedEvent::SerializeData(void* buffer) const {}

ObserveProxyEvent::ObserveProxyEvent(const PortName& port_name,
                                     const NodeName& proxy_node_name,
                                     const PortName& proxy_port_name,
                                     const NodeName& proxy_target_node_name,
                                     const PortName& proxy_target_port_name)
    : Event(Type::kObserveProxy, port_name),
      proxy_node_name_(proxy_node_name),
      proxy_port_name_(proxy_port_name),
      proxy_target_node_name_(proxy_target_node_name),
      proxy_target_port_name_(proxy_target_port_name) {}

ObserveProxyEvent::~ObserveProxyEvent() = default;

// static
ScopedEvent ObserveProxyEvent::Deserialize(const PortName& port_name,
                                           const void* buffer,
                                           size_t num_bytes) {
  if (num_bytes < sizeof(ObserveProxyEventData))
    return nullptr;

  const auto* data = static_cast<const ObserveProxyEventData*>(buffer);
  return std::make_unique<ObserveProxyEvent>(
      port_name, data->proxy_node_name, data->proxy_port_name,
      data->proxy_target_node_name, data->proxy_target_port_name);
}

size_t ObserveProxyEvent::GetSerializedDataSize() const {
  return sizeof(ObserveProxyEventData);
}

void ObserveProxyEvent::SerializeData(void* buffer) const {
  auto* data = static_cast<ObserveProxyEventData*>(buffer);
  data->proxy_node_name = proxy_node_name_;
  data->proxy_port_name = proxy_port_name_;
  data->proxy_target_node_name = proxy_target_node_name_;
  data->proxy_target_port_name = proxy_target_port_name_;
}

ScopedEvent ObserveProxyEvent::Clone() const {
  return std::make_unique<ObserveProxyEvent>(
      port_name(), proxy_node_name_, proxy_port_name_, proxy_target_node_name_,
      proxy_target_port_name_);
}

ObserveProxyAckEvent::ObserveProxyAckEvent(const PortName& port_name,
                                           uint64_t last_sequence_num)
    : Event(Type::kObserveProxyAck, port_name),
      last_sequence_num_(last_sequence_num) {}

ObserveProxyAckEvent::~ObserveProxyAckEvent() = default;

// static
ScopedEvent ObserveProxyAckEvent::Deserialize(const PortName& port_name,
                                              const void* buffer,
                                              size_t num_bytes) {
  if (num_bytes < sizeof(ObserveProxyAckEventData))
    return nullptr;

  const auto* data = static_cast<const ObserveProxyAckEventData*>(buffer);
  return std::make_unique<ObserveProxyAckEvent>(port_name,
                                                data->last_sequence_num);
}

size_t ObserveProxyAckEvent::GetSerializedDataSize() const {
  return sizeof(ObserveProxyAckEventData);
}

void ObserveProxyAckEvent::SerializeData(void* buffer) const {
  auto* data = static_cast<ObserveProxyAckEventData*>(buffer);
  data->last_sequence_num = last_sequence_num_;
}

ScopedEvent ObserveProxyAckEvent::Clone() const {
  return std::make_unique<ObserveProxyAckEvent>(port_name(),
                                                last_sequence_num_);
}

ObserveClosureEvent::ObserveClosureEvent(const PortName& port_name,
                                         uint64_t last_sequence_num)
    : Event(Type::kObserveClosure, port_name),
      last_sequence_num_(last_sequence_num) {}

ObserveClosureEvent::~ObserveClosureEvent() = default;

// static
ScopedEvent ObserveClosureEvent::Deserialize(const PortName& port_name,
                                             const void* buffer,
                                             size_t num_bytes) {
  if (num_bytes < sizeof(ObserveClosureEventData))
    return nullptr;

  const auto* data = static_cast<const ObserveClosureEventData*>(buffer);
  return std::make_unique<ObserveClosureEvent>(port_name,
                                               data->last_sequence_num);
}

size_t ObserveClosureEvent::GetSerializedDataSize() const {
  return sizeof(ObserveClosureEventData);
}

void ObserveClosureEvent::SerializeData(void* buffer) const {
  auto* data = static_cast<ObserveClosureEventData*>(buffer);
  data->last_sequence_num = last_sequence_num_;
}

ScopedEvent ObserveClosureEvent::Clone() const {
  return std::make_unique<ObserveClosureEvent>(port_name(), last_sequence_num_);
}

MergePortEvent::MergePortEvent(const PortName& port_name,
                               const PortName& new_port_name,
                               const PortDescriptor& new_port_descriptor)
    : Event(Type::kMergePort, port_name),
      new_port_name_(new_port_name),
      new_port_descriptor_(new_port_descriptor) {}

MergePortEvent::~MergePortEvent() = default;

// static
ScopedEvent MergePortEvent::Deserialize(const PortName& port_name,
                                        const void* buffer,
                                        size_t num_bytes) {
  if (num_bytes < sizeof(MergePortEventData))
    return nullptr;

  const auto* data = static_cast<const MergePortEventData*>(buffer);
  return std::make_unique<MergePortEvent>(port_name, data->new_port_name,
                                          data->new_port_descriptor);
}

size_t MergePortEvent::GetSerializedDataSize() const {
  return sizeof(MergePortEventData);
}

void MergePortEvent::SerializeData(void* buffer) const {
  auto* data = static_cast<MergePortEventData*>(buffer);
  data->new_port_name = new_port_name_;
  data->new_port_descriptor = new_port_descriptor_;
}

UserMessageReadAckRequestEvent::UserMessageReadAckRequestEvent(
    const PortName& port_name,
    uint64_t sequence_num_to_acknowledge)
    : Event(Type::kUserMessageReadAckRequest, port_name),
      sequence_num_to_acknowledge_(sequence_num_to_acknowledge) {
  DCHECK(sequence_num_to_acknowledge);
}

UserMessageReadAckRequestEvent::~UserMessageReadAckRequestEvent() = default;

// static
ScopedEvent UserMessageReadAckRequestEvent::Deserialize(
    const PortName& port_name,
    const void* buffer,
    size_t num_bytes) {
  if (num_bytes < sizeof(UserMessageReadAckRequestEventData))
    return nullptr;

  const auto* data =
      static_cast<const UserMessageReadAckRequestEventData*>(buffer);
  return std::make_unique<UserMessageReadAckRequestEvent>(
      port_name, data->sequence_num_to_acknowledge);
}

size_t UserMessageReadAckRequestEvent::GetSerializedDataSize() const {
  return sizeof(UserMessageReadAckRequestEventData);
}

void UserMessageReadAckRequestEvent::SerializeData(void* buffer) const {
  auto* data = static_cast<UserMessageReadAckRequestEventData*>(buffer);
  data->sequence_num_to_acknowledge = sequence_num_to_acknowledge_;
}

UserMessageReadAckEvent::UserMessageReadAckEvent(
    const PortName& port_name,
    uint64_t sequence_num_acknowledged)
    : Event(Type::kUserMessageReadAck, port_name),
      sequence_num_acknowledged_(sequence_num_acknowledged) {
  DCHECK(sequence_num_acknowledged);
}

UserMessageReadAckEvent::~UserMessageReadAckEvent() = default;

// static
ScopedEvent UserMessageReadAckEvent::Deserialize(const PortName& port_name,
                                                 const void* buffer,
                                                 size_t num_bytes) {
  if (num_bytes < sizeof(UserMessageReadAckEventData))
    return nullptr;

  const auto* data = static_cast<const UserMessageReadAckEventData*>(buffer);
  return std::make_unique<UserMessageReadAckEvent>(
      port_name, data->sequence_num_acknowledged);
}

size_t UserMessageReadAckEvent::GetSerializedDataSize() const {
  return sizeof(UserMessageReadAckEventData);
}

void UserMessageReadAckEvent::SerializeData(void* buffer) const {
  auto* data = static_cast<UserMessageReadAckEventData*>(buffer);
  data->sequence_num_acknowledged = sequence_num_acknowledged_;
}

}  // namespace ports
}  // namespace core
}  // namespace mojo
