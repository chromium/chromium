// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_FILTER_SOURCE_STREAM_H_
#define NET_FILTER_FILTER_SOURCE_STREAM_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/filter/source_stream.h"

namespace net {

class DrainableIOBuffer;
class IOBuffer;

// FilterSourceStream represents SourceStreams that always have an upstream
// from which undecoded input can be read. Except the ultimate upstream in
// the filter chain, all other streams should implement FilterSourceStream
// instead of SourceStream.
class NET_EXPORT_PRIVATE FilterSourceStream : public SourceStream {
 public:
  // |upstream| is the SourceStream from which |this| will read data.
  // |upstream| cannot be null.
  FilterSourceStream(SourceType type, std::unique_ptr<SourceStream> upstream);

  FilterSourceStream(const FilterSourceStream&) = delete;
  FilterSourceStream& operator=(const FilterSourceStream&) = delete;

  ~FilterSourceStream() override;

  // SourceStream implementation.
  int Read(IOBuffer* read_buffer,
           int read_buffer_size,
           CompletionOnceCallback callback) override;
  std::string Description() const override;
  bool MayHaveMoreBytes() const override;

  static SourceType ParseEncodingType(const std::string& encoding);

 private:
  enum State {
    STATE_NONE,
    // Reading data from |upstream_| into |input_buffer_|.
    STATE_READ_DATA,
    // Reading data from |upstream_| completed.
    STATE_READ_DATA_COMPLETE,
    // Filtering data contained in |input_buffer_|.
    STATE_FILTER_DATA,
    // Filtering data contained in |input_buffer_| completed.
    STATE_FILTER_DATA_COMPLETE,
    STATE_DONE,
  };

  int DoLoop(int result);
  int DoReadData();
  int DoReadDataComplete(int result);
  int DoFilterData();

  // Helper method used as a callback argument passed to |upstream_->Read()|.
  void OnIOComplete(int result);

  // Subclasses should implement this method to filter data from
  // |input_buffer| and write to |output_buffer|.
  // This method must complete synchronously (i.e. It cannot return
  // ERR_IO_PENDING). If an unrecoverable error occurred, this should return
  // ERR_CONTENT_DECODING_FAILED or a more specific error code.
  //
  // If FilterData() returns 0, *|consumed_bytes| must be equal to
  // |input_buffer_size|. Upstream EOF is reached when FilterData() is called
  // with |upstream_eof_reached| = true.
  // TODO(xunjieli): consider allowing asynchronous response via callback
  // to support off-thread decompression.
  virtual base::expected<size_t, Error> FilterData(
      IOBuffer* output_buffer,
      size_t output_buffer_size,
      IOBuffer* input_buffer,
      size_t input_buffer_size,
      size_t* consumed_bytes,
      bool upstream_eof_reached) = 0;

  // Returns a string representation of the type of this FilterSourceStream.
  // This is for UMA logging.
  virtual std::string GetTypeAsString() const = 0;

  // Returns whether |this| still needs more input data from |upstream_|.
  // By default, |this| will continue reading until |upstream_| returns an error
  // or EOF. Subclass can override this to return false to skip reading all the
  // input from |upstream_|.
  virtual bool NeedMoreData() const;

  // The SourceStream from which |this| will read data from. Data flows from
  // |upstream_| to |this_|.
  std::unique_ptr<SourceStream> upstream_;

  State next_state_ = STATE_NONE;

  // Buffer for reading data out of |upstream_| and then for use by |this|
  // before the filtered data is returned through Read().
  scoped_refptr<IOBuffer> input_buffer_;

  // Wrapper around |input_buffer_| that makes visible only the unread data.
  // Keep this as a member because subclass might not drain everything in a
  // single FilterData().
  scoped_refptr<DrainableIOBuffer> drainable_input_buffer_;

  // Not null if there is a pending Read.
  scoped_refptr<IOBuffer> output_buffer_;
  size_t output_buffer_size_ = 0;
  CompletionOnceCallback callback_;

  // Reading from |upstream_| has returned 0 byte or an error code.
  bool upstream_end_reached_ = false;
};

}  // namespace net

#endif  // NET_FILTER_FILTER_SOURCE_STREAM_H_
