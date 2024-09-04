// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_COREML_BUFFER_IMPL_COREML_H_
#define SERVICES_WEBNN_COREML_BUFFER_IMPL_COREML_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom.h"
#include "services/webnn/queueable_resource_state.h"
#include "services/webnn/webnn_buffer_impl.h"

namespace webnn {

class WebNNContextImpl;

namespace coreml {

class BufferContent;

class API_AVAILABLE(macos(12.3)) BufferImplCoreml final
    : public WebNNBufferImpl {
 public:
  static base::expected<std::unique_ptr<WebNNBufferImpl>, mojom::ErrorPtr>
  Create(mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
         WebNNContextImpl* context,
         mojom::BufferInfoPtr buffer_info);

  BufferImplCoreml(
      mojo::PendingAssociatedReceiver<mojom::WebNNBuffer> receiver,
      WebNNContextImpl* context,
      mojom::BufferInfoPtr buffer_info,
      scoped_refptr<QueueableResourceState<BufferContent>> buffer_state,
      base::PassKey<BufferImplCoreml> pass_key);

  BufferImplCoreml(const BufferImplCoreml&) = delete;
  BufferImplCoreml& operator=(const BufferImplCoreml&) = delete;
  ~BufferImplCoreml() override;

  // WebNNBufferImpl:
  void ReadBufferImpl(mojom::WebNNBuffer::ReadBufferCallback callback) override;
  void WriteBufferImpl(mojo_base::BigBuffer src_buffer) override;

  const scoped_refptr<QueueableResourceState<BufferContent>>& GetBufferState()
      const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<QueueableResourceState<BufferContent>> buffer_state_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace coreml

}  // namespace webnn

#endif  // SERVICES_WEBNN_COREML_BUFFER_IMPL_COREML_H_
