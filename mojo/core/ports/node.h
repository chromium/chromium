// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_PORTS_NODE_H_
#define MOJO_CORE_PORTS_NODE_H_

#include <stddef.h>
#include <stdint.h>

#include <queue>
#include <unordered_map>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "mojo/core/ports/event.h"
#include "mojo/core/ports/name.h"
#include "mojo/core/ports/port.h"
#include "mojo/core/ports/port_ref.h"
#include "mojo/core/ports/user_data.h"

namespace mojo {
namespace core {
namespace ports {

enum : int {
  OK = 0,
  ERROR_PORT_UNKNOWN = -10,
  ERROR_PORT_EXISTS = -11,
  ERROR_PORT_STATE_UNEXPECTED = -12,
  ERROR_PORT_CANNOT_SEND_SELF = -13,
  ERROR_PORT_PEER_CLOSED = -14,
  ERROR_PORT_CANNOT_SEND_PEER = -15,
  ERROR_NOT_IMPLEMENTED = -100,
};

struct PortStatus {
  bool has_messages;
  bool receiving_messages;
  bool peer_closed;
  bool peer_remote;
  size_t queued_message_count;
  size_t queued_num_bytes;
  size_t unacknowledged_message_count;
};

class MessageFilter;
class NodeDelegate;

// A Node maintains a collection of Ports (see port.h) indexed by unique 128-bit
// addresses (names), performing routing and processing of events among the
// Ports within the Node and to or from other Nodes in the system. Typically
// (and practically, in all uses today) there is a single Node per system
// process. Thus a Node boundary effectively models a process boundary.
//
// New Ports can be created uninitialized using CreateUninitializedPort (and
// later initialized using InitializePort), or created in a fully initialized
// state using CreatePortPair(). Initialized ports have exactly one conjugate
// port which is the ultimate receiver of any user messages sent by that port.
// See SendUserMessage().
//
// In addition to routing user message events, various control events are used
// by Nodes to coordinate Port behavior and lifetime within and across Nodes.
// See Event documentation for description of different types of events used by
// a Node to coordinate behavior.
class COMPONENT_EXPORT(MOJO_CORE_PORTS) Node {
 public:
  enum class ShutdownPolicy {
    DONT_ALLOW_LOCAL_PORTS,
    ALLOW_LOCAL_PORTS,
  };

  // Does not take ownership of the delegate.
  Node(const NodeName& name, NodeDelegate* delegate);
  ~Node();

  // Returns true iff there are no open ports referring to another node or ports
  // in the process of being transferred from this node to another. If this
  // returns false, then to ensure clean shutdown, it is necessary to keep the
  // node alive and continue routing messages to it via AcceptMessage. This
  // method may be called again after AcceptMessage to check if the Node is now
  // ready to be destroyed.
  //
  // If |policy| is set to |ShutdownPolicy::ALLOW_LOCAL_PORTS|, this will return
  // |true| even if some ports remain alive, as long as none of them are proxies
  // to another node.
  bool CanShutdownCleanly(
      ShutdownPolicy policy = ShutdownPolicy::DONT_ALLOW_LOCAL_PORTS);

  // Lookup the named port.
  int GetPort(const PortName& port_name, PortRef* port_ref);

  // Creates a port on this node. Before the port can be used, it must be
  // initialized using InitializePort. This method is useful for bootstrapping
  // a connection between two nodes. Generally, ports are created using
  // CreatePortPair instead.
  int CreateUninitializedPort(PortRef* port_ref);

  // Initializes a newly created port.
  int InitializePort(const PortRef& port_ref,
                     const NodeName& peer_node_name,
                     const PortName& peer_port_name);

  // Generates a new connected pair of ports bound to this node. These ports
  // are initialized and ready to go.
  int CreatePortPair(PortRef* port0_ref, PortRef* port1_ref);

  // User data associated with the port.
  int SetUserData(const PortRef& port_ref, scoped_refptr<UserData> user_data);
  int GetUserData(const PortRef& port_ref, scoped_refptr<UserData>* user_data);

  // Prevents further messages from being sent from this port or delivered to
  // this port. The port is removed, and the port's peer is notified of the
  // closure after it has consumed all pending messages.
  int ClosePort(const PortRef& port_ref);

  // Returns the current status of the port.
  int GetStatus(const PortRef& port_ref, PortStatus* port_status);

  // Returns the next available message on the specified port or returns a null
  // message if there are none available. Returns ERROR_PORT_PEER_CLOSED to
  // indicate that this port's peer has closed. In such cases GetMessage may
  // be called until it yields a null message, indicating that no more messages
  // may be read from the port.
  //
  // If |filter| is non-null, the next available message is returned only if it
  // is matched by the filter. If the provided filter does not match the next
  // available message, GetMessage() behaves as if there is no message
  // available. Ownership of |filter| is not taken, and it must outlive the
  // extent of this call.
  int GetMessage(const PortRef& port_ref,
                 std::unique_ptr<UserMessageEvent>* message,
                 MessageFilter* filter);

  // Sends a message from the specified port to its peer. Note that the message
  // notification may arrive synchronously (via PortStatusChanged() on the
  // delegate) if the peer is local to this Node.
  int SendUserMessage(const PortRef& port_ref,
                      std::unique_ptr<UserMessageEvent> message);

  // Makes the port send acknowledge requests to its conjugate to acknowledge
  // at least every |sequence_number_acknowledge_interval| messages as they're
  // read from the conjugate. The number of unacknowledged messages is exposed
  // in the |unacknowledged_message_count| field of PortStatus. This allows
  // bounding the number of unread and/or in-transit messages from this port
  // to its conjugate between zero and |unacknowledged_message_count|.
  int SetAcknowledgeRequestInterval(
      const PortRef& port_ref,
      uint64_t sequence_number_acknowledge_interval);

  // Corresponding to NodeDelegate::ForwardEvent.
  int AcceptEvent(ScopedEvent event);

  // Called to merge two ports with each other. If you have two independent
  // port pairs A <=> B and C <=> D, the net result of merging B and C is a
  // single connected port pair A <=> D.
  //
  // Note that the behavior of this operation is undefined if either port to be
  // merged (B or C above) has ever been read from or written to directly, and
  // this must ONLY be called on one side of the merge, though it doesn't matter
  // which side.
  //
  // It is safe for the non-merged peers (A and D above) to be transferred,
  // closed, and/or written to before, during, or after the merge.
  int MergePorts(const PortRef& port_ref,
                 const NodeName& destination_node_name,
                 const PortName& destination_port_name);

  // Like above but merges two ports local to this node. Because both ports are
  // local this can also verify that neither port has been written to before the
  // merge. If this fails for any reason, both ports are closed. Otherwise OK
  // is returned and the ports' receiving peers are connected to each other.
  int MergeLocalPorts(const PortRef& port0_ref, const PortRef& port1_ref);

  // Called to inform this node that communication with another node is lost
  // indefinitely. This triggers cleanup of ports bound to this node.
  int LostConnectionToNode(const NodeName& node_name);

 private:
  // Helper to ensure that a Node always calls into its delegate safely, i.e.
  // without holding any internal locks.
  class DelegateHolder {
   public:
    DelegateHolder(Node* node, NodeDelegate* delegate);
    ~DelegateHolder();

    NodeDelegate* operator->() const {
      EnsureSafeDelegateAccess();
      return delegate_;
    }

   private:
#if DCHECK_IS_ON()
    void EnsureSafeDelegateAccess() const;
#else
    void EnsureSafeDelegateAccess() const {}
#endif

    Node* const node_;
    NodeDelegate* const delegate_;

    DISALLOW_COPY_AND_ASSIGN(DelegateHolder);
  };

  int OnUserMessage(std::unique_ptr<UserMessageEvent> message);
  int OnPortAccepted(std::unique_ptr<PortAcceptedEvent> event);
  int OnObserveProxy(std::unique_ptr<ObserveProxyEvent> event);
  int OnObserveProxyAck(std::unique_ptr<ObserveProxyAckEvent> event);
  int OnObserveClosure(std::unique_ptr<ObserveClosureEvent> event);
  int OnMergePort(std::unique_ptr<MergePortEvent> event);
  int OnUserMessageReadAckRequest(
      std::unique_ptr<UserMessageReadAckRequestEvent> event);
  int OnUserMessageReadAck(std::unique_ptr<UserMessageReadAckEvent> event);

  int AddPortWithName(const PortName& port_name, scoped_refptr<Port> port);
  void ErasePort(const PortName& port_name);

  int SendUserMessageInternal(const PortRef& port_ref,
                              std::unique_ptr<UserMessageEvent>* message);
  int MergePortsInternal(const PortRef& port0_ref,
                         const PortRef& port1_ref,
                         bool allow_close_on_bad_state);
  void ConvertToProxy(Port* port,
                      const NodeName& to_node_name,
                      PortName* port_name,
                      Event::PortDescriptor* port_descriptor);
  int AcceptPort(const PortName& port_name,
                 const Event::PortDescriptor& port_descriptor);

  int PrepareToForwardUserMessage(const PortRef& forwarding_port_ref,
                                  Port::State expected_port_state,
                                  bool ignore_closed_peer,
                                  UserMessageEvent* message,
                                  NodeName* forward_to_node);
  int BeginProxying(const PortRef& port_ref);
  int ForwardUserMessagesFromProxy(const PortRef& port_ref);
  void InitiateProxyRemoval(const PortRef& port_ref);
  void TryRemoveProxy(const PortRef& port_ref);
  void DestroyAllPortsWithPeer(const NodeName& node_name,
                               const PortName& port_name);

  // Changes the peer node and port name referenced by |port|. Note that both
  // |ports_lock_| MUST be held through the extent of this method.
  // |local_port|'s lock must be held if and only if a reference to |local_port|
  // exist in |ports_|.
  void UpdatePortPeerAddress(const PortName& local_port_name,
                             Port* local_port,
                             const NodeName& new_peer_node,
                             const PortName& new_peer_port);

  // Removes an entry from |peer_port_map_| corresponding to |local_port|'s peer
  // address, if valid.
  void RemoveFromPeerPortMap(const PortName& local_port_name, Port* local_port);

  // Swaps the peer information for two local ports. Used during port merges.
  // Note that |ports_lock_| must be held along with each of the two port's own
  // locks, through the extent of this method.
  void SwapPortPeers(const PortName& port0_name,
                     Port* port0,
                     const PortName& port1_name,
                     Port* port1);

  // Sends an acknowledge request to the peer if the port has a non-zero
  // |sequence_num_acknowledge_interval|. This needs to be done when the port's
  // peer changes, as the previous peer proxy may not have forwarded any prior
  // acknowledge request before deleting itself.
  void MaybeResendAckRequest(const PortRef& port_ref);

  // Forwards a stored acknowledge request to the peer if the proxy has a
  // non-zero |sequence_num_acknowledge_interval|.
  void MaybeForwardAckRequest(const PortRef& port_ref);

  // Sends an acknowledge of the most recently read sequence number to the peer
  // if any messages have been read, and the port has a non-zero
  // |sequence_num_to_acknowledge|.
  void MaybeResendAck(const PortRef& port_ref);

  const NodeName name_;
  const DelegateHolder delegate_;

  // Just to clarify readability of the types below.
  using LocalPortName = PortName;
  using PeerPortName = PortName;

  // Guards access to |ports_| and |peer_port_maps_| below.
  //
  // This must never be acquired while an individual port's lock is held on the
  // same thread. Conversely, individual port locks may be acquired while this
  // one is held.
  //
  // Because UserMessage events may execute arbitrary user code during
  // destruction, it is also important to ensure that such events are never
  // destroyed while this (or any individual Port) lock is held.
  base::Lock ports_lock_;
  std::unordered_map<LocalPortName, scoped_refptr<Port>> ports_;

  // Maps a peer port name to a list of PortRefs for all local ports which have
  // the port name key designated as their peer port. The set of local ports
  // which have the same peer port is expected to always be relatively small and
  // usually 1. Hence we just use a flat_map of local PortRefs keyed on each
  // local port's name.
  using PeerPortMap =
      std::unordered_map<PeerPortName, base::flat_map<LocalPortName, PortRef>>;

  // A reverse mapping which can be used to find all local ports that reference
  // a given peer node or a local port that references a specific given peer
  // port on a peer node. The key to this map is the corresponding peer node
  // name.
  std::unordered_map<NodeName, PeerPortMap> peer_port_maps_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace ports
}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_PORTS_NODE_H_
