// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/copy_output_request_mojom_traits.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/viz/public/cpp/compositing/copy_output_result_mojom_traits.h"
#include "services/viz/public/cpp/crash_keys.h"

namespace {

// When we're sending a CopyOutputRequest, we keep the result_callback_ in a
// CopyOutputResultSenderImpl and send a PendingRemote<CopyOutputResultSender>
// to the other process. When SendResult is called, we run the stored
// result_callback_.
class CopyOutputResultSenderImpl : public viz::mojom::CopyOutputResultSender {
 public:
  CopyOutputResultSenderImpl(
      viz::CopyOutputRequest::ResultFormat result_format,
      viz::CopyOutputRequest::ResultDestination result_destination,
      viz::CopyOutputRequest::CopyOutputRequestCallback result_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : result_format_(result_format),
        result_destination_(result_destination),
        result_callback_(std::move(result_callback)),
        result_callback_task_runner_(std::move(callback_task_runner)) {
    DCHECK(result_callback_);
    DCHECK(result_callback_task_runner_);
  }

  ~CopyOutputResultSenderImpl() override {
    if (result_callback_) {
      result_callback_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(result_callback_),
                                    std::make_unique<viz::CopyOutputResult>(
                                        result_format_, result_destination_,
                                        gfx::Rect(), false)));
    }
  }

  // mojom::CopyOutputResultSender implementation.
  void SendResult(std::unique_ptr<viz::CopyOutputResult> result) override {
    TRACE_EVENT0("viz", "CopyOutputResultSenderImpl::SendResult");
    if (!result_callback_)
      return;
    result_callback_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(result_callback_), std::move(result)));
  }

 private:
  const viz::CopyOutputRequest::ResultFormat result_format_;
  const viz::CopyOutputRequest::ResultDestination result_destination_;
  viz::CopyOutputRequest::CopyOutputRequestCallback result_callback_;
  scoped_refptr<base::SequencedTaskRunner> result_callback_task_runner_;
};

void SendResult(
    mojo::PendingRemote<viz::mojom::CopyOutputResultSender> pending_remote,
    std::unique_ptr<viz::CopyOutputResult> result) {
  TRACE_EVENT0("viz", "viz::mojom::CopyOutputResultSender::SendResult");
  mojo::Remote<viz::mojom::CopyOutputResultSender> remote(
      std::move(pending_remote));
  remote->SendResult(std::move(result));
}

#if BUILDFLAG(IS_ANDROID)

class ScopedThreadType {
 public:
  explicit ScopedThreadType(base::ThreadType thread_type) {
    former_thread_type_ = base::PlatformThread::GetCurrentThreadType();
    base::PlatformThread::SetCurrentThreadType(thread_type);
  }
  ~ScopedThreadType() {
    base::PlatformThread::SetCurrentThreadType(former_thread_type_);
  }

 private:
  base::ThreadType former_thread_type_;
};

// On Android, we don't currently have threadpools with background priority.
// This function momentarily reduces the priority of the thread where the task
// is running.
// https://crbug.com/384902323
void SendResultBackgroundPriority(
    mojo::PendingRemote<viz::mojom::CopyOutputResultSender> pending_remote,
    std::unique_ptr<viz::CopyOutputResult> result) {
  TRACE_EVENT0(
      "viz",
      "viz::mojom::CopyOutputResultSender::SendResultBackgroundPriority");
  ScopedThreadType scoped_thread_type(base::ThreadType::kBackground);
  SendResult(std::move(pending_remote), std::move(result));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

namespace mojo {

// static
viz::mojom::CopyOutputRequestIpcPriority
EnumTraits<viz::mojom::CopyOutputRequestIpcPriority,
           viz::CopyOutputRequest::IpcPriority>::
    ToMojom(viz::CopyOutputRequest::IpcPriority priority) {
  switch (priority) {
    case viz::CopyOutputRequest::IpcPriority::kDefault:
      return viz::mojom::CopyOutputRequestIpcPriority::kDefault;
    case viz::CopyOutputRequest::IpcPriority::kBackground:
      return viz::mojom::CopyOutputRequestIpcPriority::kBackground;
  }
  NOTREACHED();
}

// static
bool EnumTraits<viz::mojom::CopyOutputRequestIpcPriority,
                viz::CopyOutputRequest::IpcPriority>::
    FromMojom(viz::mojom::CopyOutputRequestIpcPriority input,
              viz::CopyOutputRequest::IpcPriority* out) {
  switch (input) {
    case viz::mojom::CopyOutputRequestIpcPriority::kDefault:
      *out = viz::CopyOutputRequest::IpcPriority::kDefault;
      return true;
    case viz::mojom::CopyOutputRequestIpcPriority::kBackground:
      *out = viz::CopyOutputRequest::IpcPriority::kBackground;
      return true;
  }
  return false;
}

// static
mojo::PendingRemote<viz::mojom::CopyOutputResultSender>
StructTraits<viz::mojom::CopyOutputRequestDataView,
             std::unique_ptr<viz::CopyOutputRequest>>::
    result_sender(const std::unique_ptr<viz::CopyOutputRequest>& request) {
  mojo::PendingRemote<viz::mojom::CopyOutputResultSender> result_sender;
  auto pending_receiver = result_sender.InitWithNewPipeAndPassReceiver();
  // Receiving the result requires an expensive deserialize operation, so by
  // default we want the pipe to operate on the ThreadPool, and then it will
  // PostTask back to the result task runner, or the current sequence.
  auto impl = std::make_unique<CopyOutputResultSenderImpl>(
      request->result_format(), request->result_destination(),
      std::move(request->result_callback_),
      request->has_result_task_runner()
          ? request->result_task_runner_
          : base::SequencedTaskRunner::GetCurrentDefault());
  auto runner = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<CopyOutputResultSenderImpl> impl,
             mojo::PendingReceiver<viz::mojom::CopyOutputResultSender>
                 receiver) {
            MakeSelfOwnedReceiver(std::move(impl), std::move(receiver));
          },
          std::move(impl), std::move(pending_receiver)));
  return result_sender;
}

// static
bool StructTraits<viz::mojom::CopyOutputRequestDataView,
                  std::unique_ptr<viz::CopyOutputRequest>>::
    Read(viz::mojom::CopyOutputRequestDataView data,
         std::unique_ptr<viz::CopyOutputRequest>* out_p) {
  viz::CopyOutputRequest::ResultFormat result_format;
  if (!data.ReadResultFormat(&result_format))
    return false;

  viz::CopyOutputRequest::ResultDestination result_destination;
  if (!data.ReadResultDestination(&result_destination))
    return false;

  auto result_sender = data.TakeResultSender<
      mojo::PendingRemote<viz::mojom::CopyOutputResultSender>>();

  viz::CopyOutputRequest::IpcPriority ipc_priority;
  if (!data.ReadIpcPriority(&ipc_priority)) {
    return false;
  }

  auto callback = SendResult;
#if BUILDFLAG(IS_ANDROID)
  if (ipc_priority == viz::CopyOutputRequest::IpcPriority::kBackground) {
    callback = SendResultBackgroundPriority;
  }
#endif
  auto request = std::make_unique<viz::CopyOutputRequest>(
      result_format, result_destination,
      base::BindOnce(callback, std::move(result_sender)));

  request->set_ipc_priority(ipc_priority);

  gfx::Vector2d scale_from;
  if (!data.ReadScaleFrom(&scale_from))
    return false;
  if (scale_from.x() <= 0) {
    viz::SetDeserializationCrashKeyString("Invalid readback scale from x");
    return false;
  }
  if (scale_from.y() <= 0) {
    viz::SetDeserializationCrashKeyString("Invalid readback scale from y");
    return false;
  }
  gfx::Vector2d scale_to;
  if (!data.ReadScaleTo(&scale_to))
    return false;
  if (scale_to.x() <= 0) {
    viz::SetDeserializationCrashKeyString("Invalid readback scale to x");
    return false;
  }
  if (scale_to.y() <= 0) {
    viz::SetDeserializationCrashKeyString("Invalid readback scale to y");
    return false;
  }
  request->SetScaleRatio(scale_from, scale_to);

  if (!data.ReadSource(&request->source_) || !data.ReadArea(&request->area_) ||
      !data.ReadResultSelection(&request->result_selection_)) {
    return false;
  }

  *out_p = std::move(request);

  return true;
}

}  // namespace mojo
