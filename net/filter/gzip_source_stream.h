// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FILTER_GZIP_SOURCE_STREAM_H_
#define NET_FILTER_GZIP_SOURCE_STREAM_H_

#include <memory>
#include <string>

#include "net/base/net_export.h"
#include "net/filter/filter_source_stream.h"
#include "net/filter/gzip_header.h"

typedef struct z_stream_s z_stream;

namespace net {

class IOBuffer;

// GZipSourceStream applies gzip and deflate content encoding/decoding to a data
// stream. As specified by HTTP 1.1, with gzip encoding the content is
// wrapped with a gzip header, and with deflate encoding the content is in
// a raw, headerless DEFLATE stream.
//
// Internally GZipSourceStream uses zlib inflate to do decoding.
//
class NET_EXPORT_PRIVATE GzipSourceStream : public FilterSourceStream {
 public:
  GzipSourceStream(const GzipSourceStream&) = delete;
  GzipSourceStream& operator=(const GzipSourceStream&) = delete;

  ~GzipSourceStream() override;

  // Creates a GzipSourceStream. Return nullptr if initialization fails.
  static std::unique_ptr<GzipSourceStream> Create(
      std::unique_ptr<SourceStream> previous,
      SourceStream::SourceType type);

 private:
  enum InputState {
    // Starts processing the input stream. Checks whether the stream is valid
    // and whether a fallback to plain data is needed.
    STATE_START,
    // Gzip header of the input stream is being processed.
    STATE_GZIP_HEADER,
    // Deflate responses may or may not have a zlib header. In this state until
    // enough has been inflated that this stream most likely has a zlib header,
    // or until a zlib header has been added. Data is appended to |replay_data_|
    // in case it needs to be replayed after adding a header.
    STATE_SNIFFING_DEFLATE_HEADER,
    // If a zlib header has to be added to the response, this state will replay
    // data passed to inflate before it was determined that no zlib header was
    // present.
    // See https://crbug.com/677001
    STATE_REPLAY_DATA,
    // The input stream is being decoded.
    STATE_COMPRESSED_BODY,
    // Gzip footer of the input stream is being processed.
    STATE_GZIP_FOOTER,
    // The end of the gzipped body has been reached. If any extra bytes are
    // received, just silently ignore them. Doing this, rather than failing the
    // request or passing the extra bytes alone with the rest of the response
    // body, matches the behavior of other browsers.
    STATE_IGNORING_EXTRA_BYTES,
  };

  GzipSourceStream(std::unique_ptr<SourceStream> previous,
                   SourceStream::SourceType type);

  // Returns true if initialization is successful, false otherwise.
  // For instance, this method returns false if there is not enough memory or
  // if there is a version mismatch.
  bool Init();

  // SourceStream implementation
  std::string GetTypeAsString() const override;
  base::expected<size_t, Error> FilterData(IOBuffer* output_buffer,
                                           size_t output_buffer_size,
                                           IOBuffer* input_buffer,
                                           size_t input_buffer_size,
                                           size_t* consumed_bytes,
                                           bool upstream_end_reached) override;

  // Inserts a zlib header to the data stream before calling zlib inflate.
  // This is used to work around server bugs. The function returns true on
  // success.
  bool InsertZlibHeader();

  // The control block of zlib which actually does the decoding.
  // This data structure is initialized by Init and updated only by
  // FilterData(), with InsertZlibHeader() being the exception as a workaround.
  std::unique_ptr<z_stream> zlib_stream_;

  // While in STATE_SNIFFING_DEFLATE_HEADER, it may be determined that a zlib
  // header needs to be added, and all received data needs to be replayed. In
  // that case, this buffer holds the data to be replayed.
  std::string replay_data_;

  // Used to parse the gzip header in gzip stream.
  // It is used when the decoding mode is GZIP_SOURCE_STREAM_GZIP.
  GZipHeader gzip_header_;

  // Tracks how many bytes of gzip footer are yet to be filtered.
  size_t gzip_footer_bytes_left_ = 0;

  // Tracks the state of the input stream.
  InputState input_state_ = STATE_START;

  // Used when replaying data.
  InputState replay_state_ = STATE_COMPRESSED_BODY;
};

}  // namespace net

#endif  // NET_FILTER_GZIP_SOURCE_STREAM_H__
