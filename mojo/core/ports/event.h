// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_EVENT_H_
#define MOJO_CORE_PORTS_EVENT_H_

#include <stdint.h>

#include <vector>

#include "base/check_op.h"
#include "base/component_export.h"
#include "base/memory/ptr_util.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/ports/user_message.h"

namespace mojo {
namespace core {
namespace ports {

class Event;

using ScopedEvent = std::unique_ptr<Event>;

// A Event is the fundamental unit of operation and communication within and
// between Nodes.
class COMPONENT_EXPORT(MOJO_CORE_PORTS) Event {
 public:
  enum Type : uint32_t {
    // A user message event contains arbitrary user-specified payload data
    // which may include any number of ports and/or system handles (e.g. FDs).
    kUserMessage,

    // When a Node receives a user message with one or more ports attached, it
    // sends back an instance of this event for every attached port to indicate
    // that the port has been accepted by its destination node.
    kPortAccepted,

    // This event begins circulation any time a port enters a proxying state. It
    // may be re-circulated in certain edge cases, but the ultimate purpose of
    // the event is to ensure that every port along a route is (if necessary)
    // aware that the proxying port is indeed proxying (and to where) so that it
    // can begin to be bypassed along the route.
    kObserveProxy,

    // An event used to acknowledge to a proxy that all concerned nodes and
    // ports are aware of its proxying state and that no more user messages will
    // be routed to it beyond a given final sequence number.
    kObserveProxyAck,

    // Indicates that a port has been closed. This event fully circulates a
    // route to ensure that all ports are aware of closure.
    kObserveClosure,

    // Used to request the merging of two routes via two sacrificial receiving
    // ports, one from each route.
    kMergePort,

    // Used to request that the conjugate port acknowledges read messages by
    // sending back a UserMessageReadAck.
    kUserMessageReadAckRequest,

    // Used to acknowledge read messages to the conjugate.
    kUserMessageReadAck,

    // Used to update the previous node and port name of a port.
    kUpdatePreviousPeer,
  };

#pragma pack(push, 1)
  struct PortDescriptor {
    PortDescriptor();

    NodeName peer_node_name;
    PortName peer_port_name;
    NodeName referring_node_name;
    PortName referring_port_name;
    uint64_t next_sequence_num_to_send;
    uint64_t next_sequence_num_to_receive;
    uint64_t last_sequence_num_to_receive;
    bool peer_closed;
    char padding[7];
  };
#pragma pack(pop)

  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;

  virtual ~Event();

  static ScopedEvent Deserialize(const void* buffer, size_t num_bytes);

  template <typename T>
  static std::unique_ptr<T> Cast(ScopedEvent* event) {
    return base::WrapUnique(static_cast<T*>(event->release()));
  }

  Type type() const { return type_; }
  const PortName& port_name() const { return port_name_; }
  void set_port_name(const PortName& port_name) { port_name_ = port_name; }

  size_t GetSerializedSize() const;
  void Serialize(void* buffer) const;
  virtual ScopedEvent CloneForBroadcast() const;

  const PortName& from_port() const { return from_port_; }
  void set_from_port(const PortName& from_port) { from_port_ = from_port; }

  uint64_t control_sequence_num() const { return control_sequence_num_; }
  void set_control_sequence_num(uint64_t control_sequence_num) {
    control_sequence_num_ = control_sequence_num;
  }

 protected:
  Event(Type type,
        const PortName& port_name,
        const PortName& from_port,
        uint64_t control_sequence_num);

  virtual size_t GetSerializedDataSize() const = 0;
  virtual void SerializeData(void* buffer) const = 0;

 private:
  const Type type_;
  PortName port_name_;
  PortName from_port_;
  uint64_t control_sequence_num_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) UserMessageEvent : public Event {
 public:
  explicit UserMessageEvent(size_t num_ports);

  UserMessageEvent(const UserMessageEvent&) = delete;
  UserMessageEvent& operator=(const UserMessageEvent&) = delete;

  ~UserMessageEvent() override;

  bool HasMessage() const { return !!message_; }
  void AttachMessage(std::unique_ptr<UserMessage> message);

  template <typename T>
  T* GetMessage() {
    DCHECK(HasMessage());
    DCHECK_EQ(&T::kUserMessageTypeInfo, message_->type_info());
    return static_cast<T*>(message_.get());
  }

  template <typename T>
  const T* GetMessage() const {
    DCHECK(HasMessage());
    DCHECK_EQ(&T::kUserMessageTypeInfo, message_->type_info());
    return static_cast<const T*>(message_.get());
  }

  void ReservePorts(size_t num_ports);
  bool NotifyWillBeRoutedExternally();

  uint64_t sequence_num() const { return sequence_num_; }
  void set_sequence_num(uint64_t sequence_num) { sequence_num_ = sequence_num; }

  size_t num_ports() const { return ports_.size(); }
  PortDescriptor* port_descriptors() { return port_descriptors_.data(); }
  PortName* ports() { return ports_.data(); }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

  size_t GetSizeIfSerialized() const;

 private:
  UserMessageEvent(const PortName& port_name,
                   const PortName& from_port,
                   uint64_t control_sequence_num,
                   uint64_t sequence_num);

  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  uint64_t sequence_num_ = 0;
  std::vector<PortDescriptor> port_descriptors_;
  std::vector<PortName> ports_;
  std::unique_ptr<UserMessage> message_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) PortAcceptedEvent : public Event {
 public:
  explicit PortAcceptedEvent(const PortName& port_name,
                             const PortName& from_port,
                             uint64_t control_sequence_num);

  PortAcceptedEvent(const PortAcceptedEvent&) = delete;
  PortAcceptedEvent& operator=(const PortAcceptedEvent&) = delete;

  ~PortAcceptedEvent() override;

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) ObserveProxyEvent : public Event {
 public:
  ObserveProxyEvent(const PortName& port_name,
                    const PortName& from_port,
                    uint64_t control_sequence_num,
                    const NodeName& proxy_node_name,
                    const PortName& proxy_port_name,
                    const NodeName& proxy_target_node_name,
                    const PortName& proxy_target_port_name);

  ObserveProxyEvent(const ObserveProxyEvent&) = delete;
  ObserveProxyEvent& operator=(const ObserveProxyEvent&) = delete;

  ~ObserveProxyEvent() override;

  const NodeName& proxy_node_name() const { return proxy_node_name_; }
  const PortName& proxy_port_name() const { return proxy_port_name_; }
  const NodeName& proxy_target_node_name() const {
    return proxy_target_node_name_;
  }
  const PortName& proxy_target_port_name() const {
    return proxy_target_port_name_;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;
  ScopedEvent CloneForBroadcast() const override;

  const NodeName proxy_node_name_;
  const PortName proxy_port_name_;
  const NodeName proxy_target_node_name_;
  const PortName proxy_target_port_name_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) ObserveProxyAckEvent : public Event {
 public:
  ObserveProxyAckEvent(const PortName& port_name,
                       const PortName& from_port,
                       uint64_t control_sequence_num,
                       uint64_t last_sequence_num);

  ObserveProxyAckEvent(const ObserveProxyAckEvent&) = delete;
  ObserveProxyAckEvent& operator=(const ObserveProxyAckEvent&) = delete;

  ~ObserveProxyAckEvent() override;

  uint64_t last_sequence_num() const { return last_sequence_num_; }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  const uint64_t last_sequence_num_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) ObserveClosureEvent : public Event {
 public:
  ObserveClosureEvent(const PortName& port_name,
                      const PortName& from_port,
                      uint64_t control_sequence_num,
                      uint64_t last_sequence_num);

  ObserveClosureEvent(const ObserveClosureEvent&) = delete;
  ObserveClosureEvent& operator=(const ObserveClosureEvent&) = delete;

  ~ObserveClosureEvent() override;

  uint64_t last_sequence_num() const { return last_sequence_num_; }
  void set_last_sequence_num(uint64_t last_sequence_num) {
    last_sequence_num_ = last_sequence_num;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  uint64_t last_sequence_num_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) MergePortEvent : public Event {
 public:
  MergePortEvent(const PortName& port_name,
                 const PortName& from_port,
                 uint64_t control_sequence_num,
                 const PortName& new_port_name,
                 const PortDescriptor& new_port_descriptor);

  MergePortEvent(const MergePortEvent&) = delete;
  MergePortEvent& operator=(const MergePortEvent&) = delete;

  ~MergePortEvent() override;

  const PortName& new_port_name() const { return new_port_name_; }
  const PortDescriptor& new_port_descriptor() const {
    return new_port_descriptor_;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  const PortName new_port_name_;
  const PortDescriptor new_port_descriptor_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) UserMessageReadAckRequestEvent
    : public Event {
 public:
  UserMessageReadAckRequestEvent(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 uint64_t sequence_num_to_acknowledge);
  ~UserMessageReadAckRequestEvent() override;

  uint64_t sequence_num_to_acknowledge() const {
    return sequence_num_to_acknowledge_;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  uint64_t sequence_num_to_acknowledge_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) UserMessageReadAckEvent : public Event {
 public:
  UserMessageReadAckEvent(const PortName& port_name,
                          const PortName& from_port,
                          uint64_t control_sequence_num,
                          uint64_t sequence_num_acknowledged);
  ~UserMessageReadAckEvent() override;

  uint64_t sequence_num_acknowledged() const {
    return sequence_num_acknowledged_;
  }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  uint64_t sequence_num_acknowledged_;
};

class COMPONENT_EXPORT(MOJO_CORE_PORTS) UpdatePreviousPeerEvent : public Event {
 public:
  UpdatePreviousPeerEvent(const PortName& port_name,
                          const PortName& from_port,
                          uint64_t control_sequence_num,
                          const NodeName& new_node_name,
                          const PortName& new_port_name);
  ~UpdatePreviousPeerEvent() override;

  const NodeName& new_node_name() const { return new_node_name_; }

  const PortName& new_port_name() const { return new_port_name_; }

  static ScopedEvent Deserialize(const PortName& port_name,
                                 const PortName& from_port,
                                 uint64_t control_sequence_num,
                                 const void* buffer,
                                 size_t num_bytes);

 private:
  size_t GetSerializedDataSize() const override;
  void SerializeData(void* buffer) const override;

  const NodeName new_node_name_;
  const PortName new_port_name_;
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_EVENT_H_
