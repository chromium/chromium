// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_DRAINED_DATA_PIPE_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_DRAINED_DATA_PIPE_CLIENT_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class BlobDataHandle;

// Receives what the user of a data pipe drained from a BytesConsumer with
// DrainAsDataPipe() does with the pipe contents. The consumer does not see the
// bytes anymore once its pipe is drained, so this is the only way for it to
// still expose them to interested observers, e.g. DevTools.
class PLATFORM_EXPORT DrainedDataPipeClient : public GarbageCollectedMixin {
 public:
  virtual ~DrainedDataPipeClient() = default;

  // Called when the pipe contents are being turned into a blob.
  // DidCreateBlobFromDrainedDataPipe() follows once the blob is created, with
  // a null handle if the creation fails or is canceled.
  virtual void WillCreateBlobFromDrainedDataPipe() = 0;
  virtual void DidCreateBlobFromDrainedDataPipe(
      const scoped_refptr<BlobDataHandle>&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_DRAINED_DATA_PIPE_CLIENT_H_
