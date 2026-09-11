// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/responder_thunk.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace {

void DetermineIfEndpointIsConnected(
    const base::WeakPtr<InterfaceEndpointClient>& client,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(client && !client->encountered_error());
}

}  // namespace

namespace internal {

ResponderThunk::ResponderThunk(
    const base::WeakPtr<InterfaceEndpointClient>& endpoint_client,
    scoped_refptr<base::SequencedTaskRunner> runner)
    : endpoint_client_(endpoint_client), task_runner_(std::move(runner)) {}

ResponderThunk::~ResponderThunk() {
  if (!accept_was_invoked_) {
    // The Service handled a message that was expecting a response
    // but did not send a response.
    // We raise an error to signal the calling application that an error
    // condition occurred. Without this the calling application would have no
    // way of knowing it should stop waiting for a response.
    if (task_runner_->RunsTasksInCurrentSequence()) {
      // Please note that even if this code is run from a different task
      // runner on the same thread as |task_runner_|, it is okay to directly
      // call InterfaceEndpointClient::RaiseError(), because it will raise
      // error from the correct task runner asynchronously.
      if (endpoint_client_) {
        endpoint_client_->RaiseError();
      }
    } else {
      // Instantiate a ScopedFizzleBlockShutdownTasks to allow this PostTask
      // to fizzle if it happens after shutdown and the endpoint is bound to a
      // BLOCK_SHUTDOWN sequence. ref. crbug.com/1442134
      base::ThreadPoolInstance::ScopedFizzleBlockShutdownTasks fizzler;
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&InterfaceEndpointClient::RaiseError,
                                    endpoint_client_));
    }
  }
}

bool ResponderThunk::Accept(Message* message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  accept_was_invoked_ = true;
  DCHECK(message->has_flag(Message::kFlagIsResponse));

  bool result = false;

  if (endpoint_client_) {
    result = endpoint_client_->Accept(message);
  }

  return result;
}

bool ResponderThunk::IsConnectedForTesting() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return endpoint_client_ && !endpoint_client_->encountered_error();
}

void ResponderThunk::IsConnectedAsync(base::OnceCallback<void(bool)> callback) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    DetermineIfEndpointIsConnected(endpoint_client_, std::move(callback));
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DetermineIfEndpointIsConnected,
                                  endpoint_client_, std::move(callback)));
  }
}

}  // namespace internal
}  // namespace mojo
