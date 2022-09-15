// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio/async_audio_data_supplier.h"

#include "base/check_op.h"

namespace remoting {

AsyncAudioDataSupplier::GetDataRequest::GetDataRequest(void* data_arg,
                                                       size_t bytes_needed_arg)
    : data(data_arg), bytes_needed(bytes_needed_arg) {
  DCHECK(data);
  DCHECK_GT(bytes_needed, 0u);
}

AsyncAudioDataSupplier::GetDataRequest::~GetDataRequest() = default;

AsyncAudioDataSupplier::AsyncAudioDataSupplier() = default;

AsyncAudioDataSupplier::~AsyncAudioDataSupplier() = default;

}  // namespace remoting
