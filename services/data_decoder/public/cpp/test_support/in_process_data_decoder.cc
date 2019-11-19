// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

#include "base/threading/sequenced_task_runner_handle.h"

namespace data_decoder {
namespace test {

InProcessDataDecoder::InProcessDataDecoder()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  ServiceProvider::Set(this);
}

InProcessDataDecoder::~InProcessDataDecoder() {
  ServiceProvider::Set(nullptr);
}

void InProcessDataDecoder::BindDataDecoderService(
    mojo::PendingReceiver<mojom::DataDecoderService> receiver) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InProcessDataDecoder::BindDataDecoderService,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
    return;
  }

  receivers_.Add(&service_, std::move(receiver));
}

}  // namespace test
}  // namespace data_decoder
