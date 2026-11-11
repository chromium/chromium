// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_TENSOR_IMPL_TFLITE_H_
#define SERVICES_WEBNN_TFLITE_TENSOR_IMPL_TFLITE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/pass_key.h"
#include "services/webnn/public/cpp/in_renderer_tensor_handle.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-forward.h"
#include "services/webnn/webnn_tensor_impl.h"

namespace webnn {

class WebNNContextImpl;

namespace tflite {

class BufferContent;

// A simple implementation of WebNNTensor which uses normal CPU buffers
// since TFLite is currently only configured to use CPU delegates.
class TensorImplTflite final : public WebNNTensorImpl {
 public:
  static base::expected<scoped_refptr<WebNNTensorImpl>, mojom::ErrorPtr> Create(
      mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
      WebNNContextImpl& context,
      mojom::TensorInfoPtr tensor_info);

  TensorImplTflite(mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
                   WebNNContextImpl& context,
                   mojom::TensorInfoPtr tensor_info,
                   std::unique_ptr<BufferContent> buffer_content,
                   base::PassKey<TensorImplTflite>);

  TensorImplTflite(const TensorImplTflite&) = delete;
  TensorImplTflite& operator=(const TensorImplTflite&) = delete;

  const BufferContent& GetBufferContent() const;
  void SetBufferContent(std::unique_ptr<BufferContent> buffer_content);

  base::WeakPtr<TensorImplTflite> AsWeakPtr() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_factory_.GetWeakPtr();
  }

 private:
  ~TensorImplTflite() override;

  void ReadTensorImpl(ReadTensorCallback callback) override;
  void WriteTensorImpl(mojo_base::BigBuffer src_buffer) override;
  bool ImportTensorImpl(ScopedAccessPtr access) override;
  void ExportTensorImpl(ScopedAccessPtr access) override;

  std::unique_ptr<BufferContent> buffer_content_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<TensorImplTflite> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace tflite

}  // namespace webnn

#endif  // SERVICES_WEBNN_TFLITE_TENSOR_IMPL_TFLITE_H_
