// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BYTES_UPLOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BYTES_UPLOADER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/chunked_data_pipe_getter.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class CORE_EXPORT BytesUploader
    : public GarbageCollected<BytesUploader>,
      public BytesConsumer::Client,
      public network::mojom::blink::ChunkedDataPipeGetter {
  USING_PRE_FINALIZER(BytesUploader, Dispose);

 public:
  BytesUploader(
      BytesConsumer* consumer,
      mojo::PendingReceiver<network::mojom::blink::ChunkedDataPipeGetter>
          pending_receiver,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~BytesUploader() override;
  BytesUploader(const BytesUploader&) = delete;
  BytesUploader& operator=(const BytesUploader&) = delete;

  void Trace(blink::Visitor*) const override;

 private:
  // BytesConsumer::Client implementation
  void OnStateChange() override;
  String DebugName() const override { return "BytesUploader"; }

  // mojom::ChunkedDataPipeGetter implementation:
  void GetSize(GetSizeCallback get_size_callback) override;
  void StartReading(mojo::ScopedDataPipeProducerHandle upload_pipe) override;

  void OnPipeWriteable(MojoResult unused);
  void WriteDataOnPipe();

  void Close();
  // TODO(yoichio): Add a string parameter and show it on console.
  void CloseOnError();

  void Dispose();

  Member<BytesConsumer> consumer_;
  mojo::Receiver<network::mojom::blink::ChunkedDataPipeGetter> receiver_;
  mojo::ScopedDataPipeProducerHandle upload_pipe_;
  mojo::SimpleWatcher upload_pipe_watcher_;
  GetSizeCallback get_size_callback_;
  uint64_t total_size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BYTES_UPLOADER_H_
