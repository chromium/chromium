// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_CONTROLLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_CONTROLLER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class InterfaceEndpointClient;
class InterfaceEndpointController;

// An internal interface used to manage endpoints within an associated group,
// which corresponds to one end of a message pipe.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) AssociatedGroupController
    : public base::RefCountedThreadSafe<AssociatedGroupController> {
 public:
  // Associates an interface with this AssociatedGroupController's message pipe.
  // It takes ownership of |handle_to_send| and returns an interface ID that
  // could be sent by any endpoints within the same associated group.
  // If |handle_to_send| is not in pending association state, it returns
  // kInvalidInterfaceId. Otherwise, the peer handle of |handle_to_send| joins
  // the associated group and is no longer pending.
  virtual InterfaceId AssociateInterface(
      ScopedInterfaceEndpointHandle handle_to_send) = 0;

  // Creates an interface endpoint handle from a given interface ID. The handle
  // joins this associated group.
  // Typically, this method is used to (1) create an endpoint handle for the
  // primary interface; or (2) create an endpoint handle on receiving an
  // interface ID from the message pipe.
  //
  // On failure, the method returns an invalid handle. Usually that is because
  // the ID has already been used to create a handle.
  virtual ScopedInterfaceEndpointHandle CreateLocalEndpointHandle(
      InterfaceId id) = 0;

  // Closes an interface endpoint handle.
  virtual void CloseEndpointHandle(
      InterfaceId id,
      const std::optional<DisconnectReason>& reason) = 0;

  // Notifies the controller that the peer of interface `id` has been closed.
  // Normally this notification comes from a remote client on the underlying
  // pipe, but in some cases the remote client may never have been made aware of
  // the new associated interface and will not be able to send such a
  // notification.
  virtual void NotifyLocalEndpointOfPeerClosure(InterfaceId id) = 0;

  // Attaches a client to the specified endpoint to send and receive messages.
  // The returned object is still owned by the controller. It must only be used
  // on the same sequence as this call, and only before the client is detached
  // using DetachEndpointClient().
  virtual InterfaceEndpointController* AttachEndpointClient(
      const ScopedInterfaceEndpointHandle& handle,
      InterfaceEndpointClient* endpoint_client,
      scoped_refptr<base::SequencedTaskRunner> runner) = 0;

  // Detaches the client attached to the specified endpoint. It must be called
  // on the same sequence as the corresponding AttachEndpointClient() call.
  virtual void DetachEndpointClient(
      const ScopedInterfaceEndpointHandle& handle) = 0;

  // Raises an error on the underlying message pipe. It disconnects the pipe
  // and notifies all interfaces running on this pipe.
  virtual void RaiseError() = 0;

  // Indicates whether or this endpoint prefers to accept outgoing messages in
  // serializaed form only.
  virtual bool PrefersSerializedMessages() = 0;

 protected:
  friend class base::RefCountedThreadSafe<AssociatedGroupController>;

  // Creates a new ScopedInterfaceEndpointHandle within this associated group.
  ScopedInterfaceEndpointHandle CreateScopedInterfaceEndpointHandle(
      InterfaceId id);

  // Notifies that the interface represented by |handle_to_send| and its peer
  // has been associated with this AssociatedGroupController's message pipe, and
  // |handle_to_send|'s peer has joined this associated group. (Note: it is the
  // peer who has joined the associated group; |handle_to_send| will be sent to
  // the remote side.)
  // Returns false if |handle_to_send|'s peer has closed.
  bool NotifyAssociation(ScopedInterfaceEndpointHandle* handle_to_send,
                         InterfaceId id);

  virtual ~AssociatedGroupController();
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_GROUP_CONTROLLER_H_
