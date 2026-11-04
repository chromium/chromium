// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_RESOURCE_BROKER_BODY_PUMP_H_
#define CONTENT_SERVICES_RESOURCE_BROKER_BODY_PUMP_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "crypto/hash.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace resource_broker {

// Copies a response body from `consumer` (the network service's end) to
// `producer` (the renderer's end) and hashes the bytes with SHA-256 as they go
// through.
//
// Used for telemetry only. Bytes are copied directly between the two pipes'
// shared-memory buffers and hashed in place without bouncing through a private
// buffer. Because the network service can still write to the upstream shared
// memory, a compromised network service could modify bytes between
// `copy_from()` and `hasher_.Update()`, so `body_hash` must never gate a
// security decision.
//
// The pump only sees the pipes. The network service closes `consumer` the
// same way after the last byte and after a network error, so a successful
// result means the pipe was drained, not that the body is complete. Callers
// must also check the loader's `OnComplete()` status.
//
// Must be created, used and destroyed on one sequence. The completion
// callback runs on that sequence and may destroy the pump.
class BodyPump {
 public:
  // Maximum bytes copied in a single `PumpData()` turn before yielding back to
  // the sequence so a fast stream cannot starve other tasks. Exposed for
  // testing.
  static constexpr size_t kMaxBytesPerTurn = 512 * 1024;

  struct Result {
    // SHA-256 of the `body_size` bytes copied to `producer` (which is the
    // prefix copied before the stop when `net_error` is not `net::OK`).
    std::array<uint8_t, crypto::hash::kSha256Size> body_hash = {};
    // Number of bytes committed to `producer`.
    uint64_t body_size = 0;
    // Wall time from `Start()` to completion.
    base::TimeDelta duration;
    // Thread CPU time spent in `PumpData()`. Zero where thread CPU time is
    // unavailable.
    base::TimeDelta pump_cpu_time;
    // `net::OK` if `consumer` was drained to its end and everything read was
    // written to `producer`. `net::ERR_ABORTED` if the renderer closed its end
    // first, `net::ERR_FAILED` otherwise.
    int net_error = 0;
  };
  using CompletionCallback = base::OnceCallback<void(const Result&)>;

  BodyPump(mojo::ScopedDataPipeConsumerHandle consumer,
           mojo::ScopedDataPipeProducerHandle producer,
           CompletionCallback callback);

  BodyPump(const BodyPump&) = delete;
  BodyPump& operator=(const BodyPump&) = delete;

  ~BodyPump();

  // Starts copying. Must be called once. May invoke `callback` synchronously
  // before returning if a pipe is already closed or ready to finish.
  void Start();

 private:
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);
  void OnWritable(MojoResult result, const mojo::HandleSignalsState& state);
  void OnPeerClosed(MojoResult result, const mojo::HandleSignalsState& state);
  void PumpData();
  void Complete(int net_error);

  mojo::ScopedDataPipeConsumerHandle consumer_;
  mojo::ScopedDataPipeProducerHandle producer_;
  CompletionCallback callback_;

  mojo::SimpleWatcher readable_watcher_;
  mojo::SimpleWatcher writable_watcher_;
  mojo::SimpleWatcher peer_closed_watcher_;

  crypto::hash::Hasher hasher_{crypto::hash::kSha256};
  uint64_t bytes_copied_ = 0;
  base::TimeTicks start_time_;
  base::TimeDelta pump_cpu_time_;
  bool completed_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace resource_broker

#endif  // CONTENT_SERVICES_RESOURCE_BROKER_BODY_PUMP_H_
