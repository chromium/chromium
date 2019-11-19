// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/data_pipe_drainer.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"

namespace mojo {

DataPipeDrainer::DataPipeDrainer(Client* client,
                                 mojo::ScopedDataPipeConsumerHandle source)
    : client_(client),
      source_(std::move(source)),
      handle_watcher_(FROM_HERE,
                      SimpleWatcher::ArmingPolicy::AUTOMATIC,
                      base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(client_);
  handle_watcher_.Watch(source_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        base::BindRepeating(&DataPipeDrainer::WaitComplete,
                                            weak_factory_.GetWeakPtr()));
}

DataPipeDrainer::~DataPipeDrainer() {}

void DataPipeDrainer::ReadData() {
  const void* buffer = nullptr;
  uint32_t num_bytes = 0;
  MojoResult rv =
      source_->BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
  if (rv == MOJO_RESULT_OK) {
    client_->OnDataAvailable(buffer, num_bytes);
    source_->EndReadData(num_bytes);
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
