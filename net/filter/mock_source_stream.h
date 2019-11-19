// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_MOCK_SOURCE_STREAM_H_
#define NET_FILTER_MOCK_SOURCE_STREAM_H_

#include <string>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/filter/source_stream.h"

namespace net {

class IOBuffer;

// A SourceStream implementation used in tests. This allows tests to specify
// what data to return for each Read() call.
class MockSourceStream : public SourceStream {
 public:
  enum Mode {
    SYNC,
    ASYNC,
  };
  MockSourceStream();
  // The destructor will crash in debug build if there is any pending read.
  ~MockSourceStream() override;

  // SourceStream implementation
  int Read(IOBuffer* dest_buffer,
           int buffer_size,
           CompletionOnceCallback callback) override;
  std::string Description() const override;
  bool MayHaveMoreBytes() const override;

  // Enqueues a result to be returned by |Read|. This method does not make a
  // copy of |data|, so |data| must outlive this object. If |mode| is SYNC,
  // |Read| will return the supplied data synchronously; otherwise, consumer
  // needs to call |CompleteNextRead|
  void AddReadResult(const char* data, int len, Error error, Mode mode);

  // Completes a pending Read() call. Crash in debug build if there is no
  // pending read.
  void CompleteNextRead();

  // Affects behavior or AddReadResult.  When set to true, each character in
  // |data| passed to AddReadResult will be read as an individual byte, instead
  // of all at once. Default to false.
  // Note that setting it only affects future calls to AddReadResult, not
  // previous ones.
  void set_read_one_byte_at_a_time(bool read_one_byte_at_a_time) {
    read_one_byte_at_a_time_ = read_one_byte_at_a_time;
  }

  void set_always_report_has_more_bytes(bool always_report_has_more_bytes) {
    always_report_has_more_bytes_ = always_report_has_more_bytes;
  }

  // Returns true if a read is waiting to be completed.
  bool awaiting_completion() const { return awaiting_completion_; }

 private:
  struct QueuedResult {
    QueuedResult(const char* data, int len, Error error, Mode mode);

    const char* data;
    const int len;
    const Error error;
    const Mode mode;
  };

  bool read_one_byte_at_a_time_ = false;
  bool always_report_has_more_bytes_ = true;
  base::queue<QueuedResult> results_;
  bool awaiting_completion_ = false;
  scoped_refptr<IOBuffer> dest_buffer_;
  CompletionOnceCallback callback_;
  int dest_buffer_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockSourceStream);
};

}  // namespace net

#endif  // NET_FILTER_MOCK_SOURCE_STREAM_H_
