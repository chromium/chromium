// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_RESPONDER_THUNK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_RESPONDER_THUNK_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/connection_group_ref.h"

namespace mojo {

class InterfaceEndpointClient;
class Message;

namespace internal {

// Helper class for `InterfaceEndpointClient` to handle incoming messages which
// expect a response.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ResponderThunk {
 public:
  explicit ResponderThunk(
      const base::WeakPtr<InterfaceEndpointClient>& endpoint_client,
      scoped_refptr<base::SequencedTaskRunner> runner);

  ResponderThunk(const ResponderThunk&) = delete;
  ResponderThunk& operator=(const ResponderThunk&) = delete;

  ~ResponderThunk();

  // Allows this thunk to be attached to a ConnectionGroup as a means of keeping
  // the group from idling while the response is pending.
  void set_connection_group(ConnectionGroupRef connection_group) {
    connection_group_ = std::move(connection_group);
  }

  // Returns `true` if the message was accepted and false otherwise, indicating
  // that the message was invalid or malformed. Note that `message` may be
  // mutated.
  bool Accept(Message* message);

  // Returns `true` if `this` is currently bound to a MessagePipe, the pipe has
  // not been closed, and the pipe has not encountered an error.
  bool IsConnectedForTesting();

  // Determines if `this` is still bound to a message pipe and has not
  // encountered any errors. This is asynchronous but may be called from any
  // sequence. `callback` is eventually invoked from an arbitrary sequence with
  // the result of the query.
  void IsConnectedAsync(base::OnceCallback<void(bool)> callback);

 private:
  base::WeakPtr<InterfaceEndpointClient> endpoint_client_;
  bool accept_was_invoked_ = false;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ConnectionGroupRef connection_group_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_RESPONDER_THUNK_H_
