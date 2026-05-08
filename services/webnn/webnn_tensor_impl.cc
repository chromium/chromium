// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/webnn_tensor_impl.h"

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_restrictions.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "services/webnn/error.h"
#include "services/webnn/public/cpp/operand_descriptor.h"
#include "services/webnn/public/cpp/webnn_trace.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom.h"
#include "services/webnn/scoped_gpu_sequence.h"
#include "services/webnn/webnn_context_impl.h"

namespace webnn {

WebNNTensorImpl::WebNNTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
    mojom::TensorInfoPtr tensor_info)
    : WebNNObjectImpl<mojom::WebNNTensor,
                      blink::WebNNTensorToken,
                      mojo::AssociatedReceiver<mojom::WebNNTensor>>(
          std::move(receiver),
          context.mojo_task_runner(),
          context.owning_task_runner()),
      context_(context),
      descriptor_(std::move(tensor_info->descriptor)),
      usage_(std::move(tensor_info->usage)) {}

WebNNTensorImpl::WebNNTensorImpl(
    mojo::PendingAssociatedReceiver<mojom::WebNNTensor> receiver,
    WebNNContextImpl& context,
    mojom::TensorInfoPtr tensor_info,
    RepresentationPtr representation)
    : WebNNObjectImpl<mojom::WebNNTensor,
                      blink::WebNNTensorToken,
                      mojo::AssociatedReceiver<mojom::WebNNTensor>>(
          std::move(receiver),
          context.mojo_task_runner(),
          context.owning_task_runner()),
      context_(context),
      representation_(std::move(representation)),
      descriptor_(std::move(tensor_info->descriptor)),
      usage_(std::move(tensor_info->usage)) {}

WebNNTensorImpl::~WebNNTensorImpl() = default;

bool WebNNTensorImpl::IsValidWithDescriptor(
    const OperandDescriptor& descriptor) const {
  return descriptor_ == descriptor;
}

void WebNNTensorImpl::ReadTensor(ReadTensorCallback callback) {
  ScopedTrace scoped_trace("WebNNTensorImpl::ReadTensor");

  if (!usage().Has(MLTensorUsageFlags::kRead)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Ensure the Mojo callback is posted back to the task runner. Running
  // it directly on the GPU sequence can violate Mojo's sequence checks,
  // even if executing on the same thread.
  auto mojo_callback_wrapper =
      base::BindPostTask(context_->mojo_task_runner(), std::move(callback));

  // Call ReadTensorImpl() implemented by a backend.
  context_->RunOrScheduleTask(base::BindOnce(
      [](WebNNTensorImpl* self, ReadTensorCallback callback,
         ScopedTrace scoped_trace,
         mojo::ReportBadMessageCallback bad_message_cb) {
        if (self->is_exported()) {
          LOG(ERROR) << "[WebNN] Invalid to read tensor when exported.";
          std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
          return;
        }
        self->ReadTensorImpl(std::move(callback));
      },
      base::RetainedRef(this), std::move(mojo_callback_wrapper),
      std::move(scoped_trace), GetMojoReceiver().GetBadMessageCallback()));
}

void WebNNTensorImpl::WriteTensor(mojo_base::BigBuffer src_buffer) {
  ScopedTrace scoped_trace("WebNNTensorImpl::WriteTensor");

  if (!usage().Has(MLTensorUsageFlags::kWrite)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // TODO(https://crbug.com/40278771): Generate error using MLContext.
  // The size of src_buffer should be either equal to the packed byte length of
  // the tensor, or zero, which requires a valid write tensor consumer.
  if ((src_buffer.size() != PackedByteLength() && src_buffer.size() != 0) ||
      (src_buffer.size() == 0 && !context_->HasValidWriteTensorConsumer())) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Call WriteTensorImpl() implemented by a backend.
  context_->RunOrScheduleTask(base::BindOnce(
      [](WebNNTensorImpl* self, mojo_base::BigBuffer src_buffer,
         ScopedTrace scoped_trace,
         mojo::ReportBadMessageCallback bad_message_cb) {
        if (self->is_exported()) {
          LOG(ERROR) << "[WebNN] Invalid to write tensor when exported.";
          std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
          return;
        }
        self->WriteTensorImpl(std::move(src_buffer));
      },
      base::RetainedRef(this), std::move(src_buffer), std::move(scoped_trace),
      GetMojoReceiver().GetBadMessageCallback()));
}

void WebNNTensorImpl::ImportTensor(uint64_t flow_id,
                                   const gpu::SyncToken& fence) {
  ScopedTrace scoped_trace("WebNNTensorImpl::ImportTensor");

  if (!usage().Has(MLTensorUsageFlags::kWebGpuInterop)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  if (!context_->gpu_sequence()) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  // Defer the next task until the fence is released, after prior scheduled
  // tasks run.
  context_->RunOrScheduleTask(
      base::BindOnce(
          [](WebNNTensorImpl* self, ScopedTrace scoped_trace, uint64_t flow_id,
             mojo::ReportBadMessageCallback bad_message_cb) {
            if (!self->is_exported()) {
              LOG(ERROR)
                  << "[WebNN] ImportTensor called without the tensor being "
                     "exported.";
              std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
              return;
            }

            TRACE_EVENT("webnn", "WebNNTensorImpl::ImportTensorImpl",
                        perfetto::TerminatingFlow::Global(flow_id));

            if (!self->ImportTensorInternal()) {
              LOG(ERROR)
                  << "[WebNN] Failed to import tensor from shared image.";
              std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
              return;
            }
          },
          base::RetainedRef(this), std::move(scoped_trace), flow_id,
          GetMojoReceiver().GetBadMessageCallback()),
      {fence});
}

void WebNNTensorImpl::ExportTensor(uint64_t flow_id,
                                   const gpu::SyncToken& release) {
  ScopedTrace scoped_trace("WebNNTensorImpl::ExportTensor");

  if (!usage().Has(MLTensorUsageFlags::kWebGpuInterop)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  context_->RunOrScheduleTask(
      base::BindOnce(
          [](WebNNTensorImpl* self, ScopedTrace scoped_trace, uint64_t flow_id,
             mojo::ReportBadMessageCallback bad_message_cb) {
            if (self->is_exported()) {
              LOG(ERROR)
                  << "[WebNN] ExportTensor called on already exported tensor.";
              std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
              return;
            }

            TRACE_EVENT("webnn", "WebNNTensorImpl::ExportTensorImpl",
                        perfetto::TerminatingFlow::Global(flow_id));

            // End WebNN access which makes the tensor be exported.
            self->ExportTensorImpl(std::move(self->representation_access_));
          },
          base::RetainedRef(this), std::move(scoped_trace), flow_id,
          GetMojoReceiver().GetBadMessageCallback()),
      {}, release);
}

void WebNNTensorImpl::ExportTensorSync(uint64_t flow_id,
                                       const gpu::SyncToken& release,
                                       ExportTensorSyncCallback callback) {
  ScopedTrace scoped_trace("WebNNTensorImpl::ExportTensorSync");

  if (!usage().Has(MLTensorUsageFlags::kWebGpuInterop)) {
    GetMojoReceiver().ReportBadMessage(kBadMessageInvalidTensor);
    return;
  }

  context_->RunOrScheduleTask(
      base::BindOnce(
          [](WebNNTensorImpl* self, ScopedTrace scoped_trace, uint64_t flow_id,
             mojo::ReportBadMessageCallback bad_message_cb) {
            if (self->is_exported()) {
              LOG(ERROR) << "[WebNN] ExportTensorSync called on already "
                            "exported tensor.";
              std::move(bad_message_cb).Run(kBadMessageInvalidTensor);
              return;
            }

            TRACE_EVENT("webnn", "WebNNTensorImpl::ExportTensorImpl",
                        perfetto::TerminatingFlow::Global(flow_id));

            // End WebNN access which makes the tensor be exported.
            self->ExportTensorImpl(std::move(self->representation_access_));
          },
          base::RetainedRef(this), std::move(scoped_trace), flow_id,
          GetMojoReceiver().GetBadMessageCallback()),
      {}, release);

  std::move(callback).Run();
}

void WebNNTensorImpl::OnDisconnect() {
  ResetMojoReceiver();
  context_->RemoveWebNNTensorImpl(handle());
}

bool WebNNTensorImpl::ImportTensorInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!representation_) {
    LOG(ERROR) << "[WebNN] No representation for tensor to import.";
    return false;
  }

  auto begin_access = [](gpu::WebNNTensorRepresentation* representation,
                         scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return ScopedAccessPtr(representation->BeginScopedAccess().release(),
                           OnTaskRunnerDeleter(std::move(task_runner)));
  };

  ScopedAccessPtr access(nullptr, OnTaskRunnerDeleter(nullptr));

  // Shared image is thread safe, access the representation on the sequence
  // owning the tensor.
  if (representation_->is_thread_safe()) {
    access = begin_access(representation_.get(), owning_task_runner());
  } else {
    // Shared image access must be acquired on the main thread. If WebNN runs on
    // its own thread, a task is posted to the main thread and waits for
    // completion. Otherwise, if WebNN is already running on the main thread,
    // access begins immediately.
    RunOrPostTaskAndWaitOnSequence(
        context_->main_task_runner(),
        base::BindOnce(
            [](gpu::WebNNTensorRepresentation* representation,
               scoped_refptr<base::SequencedTaskRunner> task_runner,
               decltype(begin_access) begin_access_cb,
               ScopedAccessPtr* out_result) {
              *out_result =
                  begin_access_cb(representation, std::move(task_runner));
            },
            // Safe to use base::Unretained because we must run or wait for the
            // post task to complete and `this` tensor cannot destruct while the
            // task is running.
            base::Unretained(representation_.get()),
            context_->main_task_runner(), begin_access, &access));
  }

  // Nothing to import if access failed.
  if (!access) {
    return false;
  }

  return ImportTensorImpl(std::move(access));
}

// static
void WebNNTensorImpl::RunOrPostTaskAndWaitOnSequence(
    scoped_refptr<base::SequencedTaskRunner> target,
    base::OnceClosure task) {
  if (target->RunsTasksInCurrentSequence()) {
    std::move(task).Run();
    return;
  }

  base::ScopedAllowBaseSyncPrimitives allow_wait;
  base::WaitableEvent done;
  target->PostTask(FROM_HERE, base::BindOnce(
                                  [](base::OnceClosure inner_callback,
                                     base::WaitableEvent* done) {
                                    std::move(inner_callback).Run();
                                    done->Signal();
                                  },
                                  std::move(task), &done));
  done.Wait();
}

}  // namespace webnn
