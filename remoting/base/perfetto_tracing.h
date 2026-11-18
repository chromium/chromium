// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PERFETTO_TRACING_H_
#define REMOTING_BASE_PERFETTO_TRACING_H_

#include <cstdint>
#include <string_view>

namespace remoting {

// Initializes the in-process Perfetto SDK backend, registers Chromium
// `base::TrackEvent` and WebRTC track-event categories, names the current
// `ProcessTrack` (without recording command-line arguments), and records
// `ThreadTrack` descriptors for existing and future named threads via
// `base::ThreadIdNameManager`.
//
// Must be called on the main thread with a non-empty `process_name` after
// `base::ThreadPoolInstance` (or a test `TaskEnvironment`) has been started
// and, in sandboxed worker processes, before entering the sandbox. Safe to call
// multiple times on the main thread in the same process (subsequent calls
// update the `ProcessTrack` name).
void InitPerfettoTracing(std::string_view process_name);

// Sets a custom `ThreadDescriptor` name on `perfetto::ThreadTrack::Current()`
// without changing the OS thread name in `base::ThreadIdNameManager` (for
// example, naming a `ThreadPool` single-thread encoder sequence
// `"CRD VideoEncoder (screen_0)"`).
void SetCurrentThreadTrackName(std::string_view thread_name);

// Returns a non-zero 64-bit trace flow ID seeded with a random per-process
// upper-32-bit prefix so `perfetto::Flow::Global(id)` links capture and encode
// slices across both threads and multi-process worker boundaries without ID
// collisions.
uint64_t NextPerfettoFrameTraceId();

// Resets the in-process Perfetto SDK state and unregisters the
// `ThreadIdNameManager` observer installed by `InitPerfettoTracing()`. Must
// only be called in tests after all active tracing sessions have stopped.
void ResetPerfettoTracingForTesting();

}  // namespace remoting

#endif  // REMOTING_BASE_PERFETTO_TRACING_H_
