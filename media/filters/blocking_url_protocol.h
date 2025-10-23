// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_BLOCKING_URL_PROTOCOL_H_
#define MEDIA_FILTERS_BLOCKING_URL_PROTOCOL_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

class DataSource;

// An implementation of FFmpegURLProtocol that blocks until the underlying
// asynchronous DataSource::Read() operation completes. Generally constructed on
// the media thread and used by ffmpeg through the AVIO interface from a
// sequenced blocking pool.
class MEDIA_EXPORT BlockingUrlProtocol : public FFmpegURLProtocol {
 public:
  BlockingUrlProtocol() = delete;

  // Implements FFmpegURLProtocol using the given |data_source|. |error_cb| is
  // fired any time DataSource::Read() returns an error.
  BlockingUrlProtocol(DataSource* data_source,
                      const base::RepeatingClosure& error_cb);

  BlockingUrlProtocol(const BlockingUrlProtocol&) = delete;
  BlockingUrlProtocol& operator=(const BlockingUrlProtocol&) = delete;

  virtual ~BlockingUrlProtocol();

  // Aborts any pending reads by returning a read error. After this method
  // returns all subsequent calls to Read() will immediately fail. May be called
  // from any thread and upon return ensures no further use of |data_source_|.
  void Abort();

  // FFmpegURLProtocol implementation.
  int Read(int size, uint8_t* data) override;
  bool GetPosition(int64_t* position_out) override;
  bool SetPosition(int64_t position) override;
  bool GetSize(int64_t* size_out) override;
  bool IsStreaming() override;

 private:
  // Sets |last_read_bytes_| and signals the blocked thread that the read
  // has completed.
  void SignalReadCompleted(int size);

  // |data_source_lock_| allows Abort() to be called from any thread and stop
  // all outstanding access to |data_source_|. Typically Abort() is called from
  // the media thread while ffmpeg is operating on another thread.
  base::Lock data_source_lock_;
  raw_ptr<DataSource> data_source_;

  base::RepeatingClosure error_cb_;
  const bool is_streaming_;

  // Used to unblock the thread during shutdown and when reads complete.
  base::WaitableEvent aborted_;
  base::WaitableEvent read_complete_;

  // Cached number of bytes last read from the data source.
  int last_read_bytes_;

  // Cached position within the data source.
  int64_t read_position_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_BLOCKING_URL_PROTOCOL_H_
