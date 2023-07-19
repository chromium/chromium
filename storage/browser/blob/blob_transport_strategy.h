// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_STRATEGY_H_
#define STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_STRATEGY_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/blob_memory_controller.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"

namespace storage {

class BlobDataBuilder;

// This class is responsible for transporting bytes for an under-construction
// blob, using a specified transport strategy. This is used by BlobRegistryImpl
// for the actual transportation of bytes.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobTransportStrategy {
 public:
  using ResultCallback = base::OnceCallback<void(BlobStatus)>;

  // Creates a BlobTransportStrategy instance for the specified memory strategy.
  // The BlobDataBuilder and BlobStorageLimits must outlive the returned
  // BlobTransportStrategy.
  static std::unique_ptr<BlobTransportStrategy> Create(
      BlobMemoryController::Strategy strategy,
      BlobDataBuilder* builder,
      ResultCallback result_callback,
      const BlobStorageLimits& limits);
  virtual ~BlobTransportStrategy();

  // Called once for each DataElementBytes in a blob. The |bytes| passed in must
  // outlive the BlobTransportStrategy instance. If |data| is bound, this call
  // may use it to acquire the bytes asynchronously rather than reading from
  // |bytes|.
  virtual void AddBytesElement(
      blink::mojom::DataElementBytes* bytes,
      const mojo::Remote<blink::mojom::BytesProvider>& data) = 0;

  // Called when quota has been allocated and transportation should begin.
  // Implementations will call the |result_callback_| when transportation has
  // completed, or failed.
  virtual void BeginTransport(
      std::vector<BlobMemoryController::FileCreationInfo> file_infos) = 0;

 protected:
  BlobTransportStrategy(BlobDataBuilder* builder,
                        ResultCallback result_callback);

  raw_ptr<BlobDataBuilder, AcrossTasksDanglingUntriaged> builder_;
  ResultCallback result_callback_;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_BLOB_BLOB_TRANSPORT_STRATEGY_H_
