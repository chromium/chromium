// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_SOURCE_STREAM_H_
#define NET_FILTER_SOURCE_STREAM_H_

#include <string>

#include "base/functional/callback.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"

namespace net {

class IOBuffer;

// The SourceStream class implements a producer of bytes.
class NET_EXPORT_PRIVATE SourceStream {
 public:
  enum SourceType {
    TYPE_BROTLI,
    TYPE_DEFLATE,
    TYPE_GZIP,
    TYPE_ZSTD,
    TYPE_UNKNOWN,
    TYPE_NONE,
  };

  // |type| is the type of the SourceStream.
  explicit SourceStream(SourceType type);

  SourceStream(const SourceStream&) = delete;
  SourceStream& operator=(const SourceStream&) = delete;

  virtual ~SourceStream();

  // Initiaties a read from the stream.
  // If it completes synchronously, it:
  //   - Returns an int representing the number of bytes read. If 0, EOF has
  //     been reached
  //   - Bytes will be written into |*dest_buffer|
  //   - Does not call |callback|
  // If it completes asynchronously, it:
  //   - Returns ERR_IO_PENDING
  //   - Calls |callback| when it does complete, with an error code or a count
  //     of bytes read and written into |*dest_buffer|.
  // This method takes a reference to |*dest_buffer| if it completes
  // asynchronously to ensure it does not get freed mid-read.
  virtual int Read(IOBuffer* dest_buffer,
                   int buffer_size,
                   CompletionOnceCallback callback) = 0;

  // Returns a string that represents stream. This is for UMA and NetLog
  // logging.
  virtual std::string Description() const = 0;

  // Returns true if there may be more bytes to read in this source stream.
  // This is not a guarantee that there are more bytes (in the case that
  // the stream doesn't know).  However, if this returns false, then the stream
  // is guaranteed to be complete.
  virtual bool MayHaveMoreBytes() const = 0;

  SourceType type() const { return type_; }

 private:
  SourceType type_;
};

}  // namespace net

#endif  // NET_FILTER_SOURCE_STREAM_H_
