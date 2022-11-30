// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/scope_to_message_pipe.h"

namespace mojo {
namespace internal {

namespace {

void OnWatcherSignaled(std::unique_ptr<MessagePipeScoperBase> scoper,
                       MojoResult result,
                       const HandleSignalsState& state) {
  DCHECK(scoper);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK(state.peer_closed());

  // No work to do except for letting |scoper| go out of scope and be destroyed.
}

}  // namespace

MessagePipeScoperBase::MessagePipeScoperBase(ScopedMessagePipeHandle pipe)
    : pipe_(std::move(pipe)),
      pipe_watcher_(FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC) {}

MessagePipeScoperBase::~MessagePipeScoperBase() = default;

// static
void MessagePipeScoperBase::StartWatchingPipe(
    std::unique_ptr<MessagePipeScoperBase> scoper) {
  auto* unowned_scoper = scoper.get();

  // NOTE: We intentionally use base::Passed here with the owned |scoper|. The
  // way we use it here, there is no way for the watcher callback to be invoked
  // more than once. If this expectation is ever violated,
  // base::RepeatingCallback will CHECK-fail.
  unowned_scoper->pipe_watcher_.Watch(
      unowned_scoper->pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&OnWatcherSignaled, base::Passed(&scoper)));
}

}  // namespace internal
}  // namespace mojo
