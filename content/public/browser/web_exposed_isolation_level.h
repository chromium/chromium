// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_EXPOSED_ISOLATION_LEVEL_H_
#define CONTENT_PUBLIC_BROWSER_WEB_EXPOSED_ISOLATION_LEVEL_H_

namespace content {

// Reflects the web-exposed isolation properties of a given frame or worker.
// For the frame, this depends both on the process in which the frame lives, as
// well as the agent cluster into which it has been placed. For a worker, this
// depends on the process in which the worker lives.
//
// Three broad categories are possible:
//
// 1.  The frame or worker may not be isolated in a web-facing way.
//
// 2.  The frame or worker may be "cross-origin isolated", corresponding to the
//     value returned by `WorkerOrWindowGlobalScope.crossOriginIsolated`, and
//     gating the set of APIs which specify [CrossOriginIsolated] attributes.
//     The requirements for this level of isolation are described in [1] and [2]
//     below.
//
//     In practice this means that the frame or worker are guaranteed to be
//     hosted in a process that is isolated to the frame or worker's origin.
//     Additionally for frames, the frame may embed cross-origin frames and
//     workers only if they have opted in to being embedded by asserting CORS or
//     CORP headers.
//
// 3.  The frame or worker may be an "isolated application", corresponding to a
//     mostly TBD set of restrictions we're exploring in
//     https://crbug.com/1206150, and which currently gate the set of APIs
//     which specify [DirectSocketEnabled] attributes.
//
// The enum below is ordered from least-isolated to most-isolated.
//
// [1]
// https://developer.mozilla.org/en-US/docs/Web/API/WindowOrWorkerGlobalScope/crossOriginIsolated
// [2] https://w3c.github.io/webappsec-permissions-policy/
//
// NOTE: some of the information needed to fully determine a frame or worker's
// isolation status is currently not available in the browser process.
// Access to web platform API's must be checked in the renderer, with the
// WebExposedIsolationLevel on the browser side only used as a backup to
// catch misbehaving renderers.
enum class WebExposedIsolationLevel {
  // The frame or worker is not in a cross-origin isolated agent cluster. It may
  // not meet the requirements for such isolation in itself, or it may be hosted
  // in a process capable of supporting cross-origin isolation or application
  // isolation, but barred from using those capabilities by its embedder.
  kNotIsolated,

  // The frame or worker is in a cross-origin isolated process and agent
  // cluster, allowed to access web platform APIs gated on
  // [CrossOriginIsolated].
  //
  // TODO(clamy): Remove this "maybe" status once it is possible to determine
  // conclusively whether the document is capable of calling cross-origin
  // isolated APIs by examining the active document policy.
  kMaybeIsolated,
  kIsolated,

  // The frame or worker is in a cross-origin isolated process and agent cluster
  // that supports application isolation, allowing access to web platform APIs
  // gated on both [CrossOriginIsolated] and [DirectSocketEnabled].
  //
  // TODO(clamy): Remove this "maybe" status once it is possible to determine
  // conclusively whether the document is capable of calling cross-origin
  // isolated APIs by examining the active document policy.
  kMaybeIsolatedApplication,
  kIsolatedApplication
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_EXPOSED_ISOLATION_LEVEL_H_
