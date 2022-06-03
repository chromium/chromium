// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WORKER_MAIN_SCRIPT_LOADER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WORKER_MAIN_SCRIPT_LOADER_CLIENT_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

// The interface that's provided to notify the loading result of
// WorkerMainScriptLoader.
class PLATFORM_EXPORT WorkerMainScriptLoaderClient
    : public GarbageCollectedMixin {
 public:
  // Called when reading a chunk, with the chunk.
  virtual void DidReceiveData(base::span<const char> span) {}

  // Called when starting to load the body.
  virtual void OnStartLoadingBody(const ResourceResponse& resource_response) {}

  // Called when the loading completes.
  virtual void OnFinishedLoadingWorkerMainScript() {}

  // Called when the error happens.
  virtual void OnFailedLoadingWorkerMainScript() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_WORKER_MAIN_SCRIPT_LOADER_CLIENT_H_
