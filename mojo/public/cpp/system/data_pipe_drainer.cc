// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_drainer.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace mojo {

DataPipeDrainer::DataPipeDrainer(Client* client,
                                 mojo::ScopedDataPipeConsumerHandle source)
    : client_(client),
      source_(std::move(source)),
      handle_watcher_(FROM_HERE,
                      SimpleWatcher::ArmingPolicy::AUTOMATIC,
                      base::SequencedTaskRunner::GetCurrentDefault()) {
  DCHECK(client_);
  handle_watcher_.Watch(source_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        base::BindRepeating(&DataPipeDrainer::WaitComplete,
                                            weak_factory_.GetWeakPtr()));
}

DataPipeDrainer::~DataPipeDrainer() {}

void DataPipeDrainer::ReadData() {
  base::span<const uint8_t> buffer;
  MojoResult rv = source_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
  if (rv == MOJO_RESULT_OK) {
    client_->OnDataAvailable(buffer);
    source_->EndReadData(buffer.size());
  } else if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
    client_->OnDataComplete();
  } else if (rv != MOJO_RESULT_SHOULD_WAIT) {
    DCHECK(false) << "Unhandled MojoResult: " << rv;
  }
}

void DataPipeDrainer::WaitComplete(MojoResult result) {
  ReadData();
}

}  // namespace mojo
