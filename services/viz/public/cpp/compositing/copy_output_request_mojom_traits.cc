// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/copy_output_request_mojom_traits.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/viz/public/cpp/compositing/copy_output_result_mojom_traits.h"

namespace {

// When we're sending a CopyOutputRequest, we keep the result_callback_ in a
// CopyOutputResultSenderImpl and send a PendingRemote<CopyOutputResultSender>
// to the other process. When SendResult is called, we run the stored
// result_callback_.
class CopyOutputResultSenderImpl : public viz::mojom::CopyOutputResultSender {
 public:
  CopyOutputResultSenderImpl(
      viz::CopyOutputRequest::ResultFormat result_format,
      viz::CopyOutputRequest::CopyOutputRequestCallback result_callback)
      : result_format_(result_format),
        result_callback_(std::move(result_callback)) {
    DCHECK(result_callback_);
  }

  ~CopyOutputResultSenderImpl() override {
    if (result_callback_) {
      std::move(result_callback_)
          .Run(std::make_unique<viz::CopyOutputResult>(result_format_,
                                                       gfx::Rect()));
    }
  }

  // mojom::CopyOutputResultSender implementation.
  void SendResult(std::unique_ptr<viz::CopyOutputResult> result) override {
    if (!result_callback_)
      return;
    std::move(result_callback_).Run(std::move(result));
  }

 private:
  const viz::CopyOutputRequest::ResultFormat result_format_;
  viz::CopyOutputRequest::CopyOutputRequestCallback result_callback_;
};

void SendResult(
    mojo::PendingRemote<viz::mojom::CopyOutputResultSender> pending_remote,
    std::unique_ptr<viz::CopyOutputResult> result) {
  mojo::Remote<viz::mojom::CopyOutputResultSender> remote(
      std::move(pending_remote));
  remote->SendResult(std::move(result));
}

}  // namespace

namespace mojo {

// static
mojo::PendingRemote<viz::mojom::CopyOutputResultSender>
StructTraits<viz::mojom::CopyOutputRequestDataView,
             std::unique_ptr<viz::CopyOutputRequest>>::
    result_sender(const std::unique_ptr<viz::CopyOutputRequest>& request) {
  mojo::PendingRemote<viz::mojom::CopyOutputResultSender> result_sender;
  auto impl = std::make_unique<CopyOutputResultSenderImpl>(
      request->result_format(), std::move(request->result_callback_));
  MakeSelfOwnedReceiver(std::move(impl),
                        result_sender.InitWithNewPipeAndPassReceiver());
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

  auto result_sender = data.TakeResultSender<
      mojo::PendingRemote<viz::mojom::CopyOutputResultSender>>();

  auto request = std::make_unique<viz::CopyOutputRequest>(
      result_format, base::BindOnce(SendResult, base::Passed(&result_sender)));

  gfx::Vector2d scale_from;
  if (!data.ReadScaleFrom(&scale_from) || scale_from.x() <= 0 ||
      scale_from.y() <= 0) {
    return false;
  }
  gfx::Vector2d scale_to;
  if (!data.ReadScaleTo(&scale_to) || scale_to.x() <= 0 || scale_to.y() <= 0) {
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
