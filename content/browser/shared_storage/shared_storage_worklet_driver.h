// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_DRIVER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_DRIVER_H_

#include "content/common/shared_storage_worklet_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// Interface to abstract away the starting of the worklet service.
class SharedStorageWorkletDriver {
 public:
  virtual ~SharedStorageWorkletDriver() = default;

  // Called when starting the worklet service.
  virtual void StartWorkletService(
      mojo::PendingReceiver<
          shared_storage_worklet::mojom::SharedStorageWorkletService>
          pending_receiver) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_DRIVER_H_
