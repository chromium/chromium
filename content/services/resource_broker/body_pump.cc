// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/body_pump.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"

namespace resource_broker {

namespace {

// The header avoids depending on `net/` for its default.
static_assert(net::OK == 0, "Result::net_error defaults to net::OK");

base::ThreadTicks ThreadCpuTimeNow() {
  return base::ThreadTicks::IsSupported() ? base::ThreadTicks::Now()
                                          : base::ThreadTicks();
}

}  // namespace

BodyPump::BodyPump(mojo::ScopedDataPipeConsumerHandle consumer,
                   mojo::ScopedDataPipeProducerHandle producer,
                   CompletionCallback callback)
    : consumer_(std::move(consumer)),
      producer_(std::move(producer)),
      callback_(std::move(callback)),
      readable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      writable_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      peer_closed_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {
  CHECK(consumer_.is_valid());
  CHECK(producer_.is_valid());
  CHECK(callback_);
}

BodyPump::~BodyPump() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BodyPump::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(start_time_.is_null());
  start_time_ = base::TimeTicks::Now();

  // Watching only the data signals makes the watchers report
  // `MOJO_RESULT_FAILED_PRECONDITION` once the other end has closed and the
  // signal can never be satisfied again.
  const MojoResult readable_rv = readable_watcher_.Watch(
      consumer_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&BodyPump::OnReadable, base::Unretained(this)));
  CHECK_EQ(readable_rv, MOJO_RESULT_OK);

  const MojoResult writable_rv = writable_watcher_.Watch(
      producer_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&BodyPump::OnWritable, base::Unretained(this)));
  CHECK_EQ(writable_rv, MOJO_RESULT_OK);

  // The renderer closing its end is only visible on the producer, so watch it
  // separately from the manual writable watcher, which is only armed while
  // waiting for buffer space.
  const MojoResult peer_closed_rv = peer_closed_watcher_.Watch(
      producer_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&BodyPump::OnPeerClosed, base::Unretained(this)));
  CHECK_EQ(peer_closed_rv, MOJO_RESULT_OK);

  PumpData();
}

void BodyPump::OnReadable(MojoResult result, const mojo::HandleSignalsState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    // The network service closed its end and everything has been read.
    Complete(net::OK);
    return;
  }
  if (result != MOJO_RESULT_OK) {
    Complete(net::ERR_FAILED);
    return;
  }
  PumpData();
}

void BodyPump::OnWritable(MojoResult result, const mojo::HandleSignalsState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    Complete(net::ERR_ABORTED);
    return;
  }
  if (result != MOJO_RESULT_OK) {
    Complete(net::ERR_FAILED);
    return;
  }
  PumpData();
}

void BodyPump::OnPeerClosed(MojoResult result,
                            const mojo::HandleSignalsState&) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != MOJO_RESULT_OK) {
    Complete(net::ERR_FAILED);
    return;
  }
  const mojo::HandleSignalsState upstream = consumer_->QuerySignalsState();
  if (upstream.peer_closed() && !upstream.readable()) {
    Complete(net::OK);
    return;
  }
  Complete(net::ERR_ABORTED);
}

void BodyPump::PumpData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Sampled once on the first chunk so an immediate `SHOULD_WAIT` or EOF does
  // not read the thread CPU clock, and guarded with `is_null()` so an
  // uncalibrated `ThreadTicks::Now()` on Windows does not produce a bogus
  // delta.
  base::ThreadTicks turn_cpu_start;
  auto commit_turn_time = [&]() {
    if (!turn_cpu_start.is_null()) {
      pump_cpu_time_ += ThreadCpuTimeNow() - turn_cpu_start;
      turn_cpu_start = base::ThreadTicks();
    }
  };

  size_t bytes_copied_this_turn = 0;
  // Every `Complete()` call below must `return` immediately, because
  // `Complete()` may destroy `this`.
  while (true) {
    base::span<const uint8_t> read_buffer;
    MojoResult read_result =
        consumer_->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, read_buffer);
    if (read_result == MOJO_RESULT_SHOULD_WAIT) {
      commit_turn_time();
      readable_watcher_.ArmOrNotify();
      return;
    }
    if (read_result == MOJO_RESULT_FAILED_PRECONDITION) {
      commit_turn_time();
      Complete(net::OK);
      return;
    }
    if (read_result != MOJO_RESULT_OK) {
      commit_turn_time();
      Complete(net::ERR_FAILED);
      return;
    }

    base::span<uint8_t> write_buffer;
    MojoResult write_result = producer_->BeginWriteData(
        mojo::DataPipeProducerHandle::kNoSizeHint,
        MOJO_BEGIN_WRITE_DATA_FLAG_NONE, write_buffer);
    if (write_result != MOJO_RESULT_OK) {
      commit_turn_time();
      if (consumer_->EndReadData(0) != MOJO_RESULT_OK) {
        Complete(net::ERR_FAILED);
        return;
      }
      if (write_result == MOJO_RESULT_SHOULD_WAIT) {
        writable_watcher_.ArmOrNotify();
      } else if (write_result == MOJO_RESULT_FAILED_PRECONDITION) {
        Complete(net::ERR_ABORTED);
      } else {
        Complete(net::ERR_FAILED);
      }
      return;
    }

    const size_t copy_size =
        std::min({read_buffer.size(), write_buffer.size(),
                  kMaxBytesPerTurn - bytes_copied_this_turn});
    CHECK_GT(copy_size, 0u);
    if (turn_cpu_start.is_null()) {
      turn_cpu_start = ThreadCpuTimeNow();
    }

    base::span<const uint8_t> chunk = read_buffer.first(copy_size);
    write_buffer.first(copy_size).copy_from(chunk);
    // Must hash before `EndReadData()` releases the two-phase lock on
    // `read_buffer`.
    hasher_.Update(chunk);

    const MojoResult end_read = consumer_->EndReadData(copy_size);
    const MojoResult end_write = producer_->EndWriteData(copy_size);
    if (end_read != MOJO_RESULT_OK || end_write != MOJO_RESULT_OK) {
      commit_turn_time();
      Complete(net::ERR_FAILED);
      return;
    }
    bytes_copied_ += copy_size;
    bytes_copied_this_turn += copy_size;
    if (bytes_copied_this_turn >= kMaxBytesPerTurn) {
      commit_turn_time();
      readable_watcher_.ArmOrNotify();
      return;
    }
  }
}

void BodyPump::Complete(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (completed_) {
    return;
  }
  completed_ = true;

  readable_watcher_.Cancel();
  writable_watcher_.Cancel();
  peer_closed_watcher_.Cancel();
  consumer_.reset();
  producer_.reset();

  Result result;
  hasher_.Finish(result.body_hash);
  result.body_size = bytes_copied_;
  result.duration = base::TimeTicks::Now() - start_time_;
  result.pump_cpu_time = pump_cpu_time_;
  result.net_error = net_error;

  // May delete `this`. Do not access members after this call.
  std::move(callback_).Run(result);
}

}  // namespace resource_broker
