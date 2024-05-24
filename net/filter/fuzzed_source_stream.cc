// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/filter/fuzzed_source_stream.h"

#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// Common net error codes that can be returned by a SourceStream.
const Error kReadErrors[] = {OK, ERR_FAILED, ERR_CONTENT_DECODING_FAILED};

}  // namespace

FuzzedSourceStream::FuzzedSourceStream(FuzzedDataProvider* data_provider)
    : SourceStream(SourceStream::TYPE_NONE), data_provider_(data_provider) {}

FuzzedSourceStream::~FuzzedSourceStream() {
  DCHECK(!read_pending_);
}

int FuzzedSourceStream::Read(IOBuffer* buf,
                             int buf_len,
                             CompletionOnceCallback callback) {
  DCHECK(!read_pending_);
  DCHECK(!end_returned_);
  DCHECK_LE(0, buf_len);

  bool sync = data_provider_->ConsumeBool();
  int result = data_provider_->ConsumeIntegralInRange(0, buf_len);
  std::string data = data_provider_->ConsumeBytesAsString(result);
  result = data.size();

  if (result <= 0)
    result = data_provider_->PickValueInArray(kReadErrors);

  if (sync) {
    if (result > 0) {
      base::ranges::copy(data, buf->data());
    } else {
      end_returned_ = true;
    }
    return result;
  }

  scoped_refptr<IOBuffer> pending_read_buf = buf;

  read_pending_ = true;
  // |this| is owned by the caller so use base::Unretained is safe.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FuzzedSourceStream::OnReadComplete,
                                base::Unretained(this), std::move(callback),
                                data, pending_read_buf, result));
  return ERR_IO_PENDING;
}

std::string FuzzedSourceStream::Description() const {
  return "";
}

bool FuzzedSourceStream::MayHaveMoreBytes() const {
  return !end_returned_;
}

void FuzzedSourceStream::OnReadComplete(CompletionOnceCallback callback,
                                        const std::string& fuzzed_data,
                                        scoped_refptr<IOBuffer> read_buf,
                                        int result) {
  DCHECK(read_pending_);

  if (result > 0) {
    std::copy(fuzzed_data.data(), fuzzed_data.data() + result,
              read_buf->data());
  } else {
    end_returned_ = true;
  }
  read_pending_ = false;
  std::move(callback).Run(result);
}

}  // namespace net
