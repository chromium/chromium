// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_BYTES_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_BYTES_PROVIDER_H_

#include "base/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"

namespace blink {

// Implementation of the BytesProvider mojo interface, used to transport bytes
// making up a blob to the browser process, at the request of the blob service.
//
// Typical usage of this class creates and calls AppendData on one thread, and
// then transfers ownership of the class to a different thread where it will be
// bound to a mojo pipe, such that the various Request* methods are called on a
// thread that is allowed to do File IO.
class PLATFORM_EXPORT BlobBytesProvider : public mojom::blink::BytesProvider {
 public:
  // All consecutive items that are accumulate to < this number will have the
  // data appended to the same item.
  static constexpr size_t kMaxConsolidatedItemSizeInBytes = 15 * 1024;

  // Creates a new instance, and binds it on a new SequencedTaskRunner. The
  // returned instance should only be considered valid as long as the request
  // passed in to this method is still known to be valid.
  static BlobBytesProvider* CreateAndBind(
      mojo::PendingReceiver<mojom::blink::BytesProvider>);
  static std::unique_ptr<BlobBytesProvider> CreateForTesting(
      scoped_refptr<base::SequencedTaskRunner>);

  ~BlobBytesProvider() override;

  void AppendData(scoped_refptr<RawData>);
  void AppendData(base::span<const char>);

  // BytesProvider implementation:
  void RequestAsReply(RequestAsReplyCallback) override;
  void RequestAsStream(mojo::ScopedDataPipeProducerHandle) override;
  void RequestAsFile(uint64_t source_offset,
                     uint64_t source_size,
                     base::File,
                     uint64_t file_offset,
                     RequestAsFileCallback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BlobBytesProviderTest, Consolidation);

  BlobBytesProvider(scoped_refptr<base::SequencedTaskRunner>);

  // The task runner this class is bound on, as well as what is used by the
  // RequestAsStream method to monitor the data pipe.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  Vector<scoped_refptr<RawData>> data_;
  // |offsets_| always contains exactly one fewer item than |data_| (except when
  // |data_| itself is empty).
  // offsets_[x] is equal to the sum of data_[i].length for all i <= x.
  Vector<uint64_t> offsets_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_BYTES_PROVIDER_H_
