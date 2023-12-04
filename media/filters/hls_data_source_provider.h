// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
#define MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "base/types/id_type.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media {

class HlsDataSourceStream;

// Interface which can provide data sources, given a URI and an optional
// byterange. This interface should be used via `base::SequenceBound` to proxy
// requests across the media thread and the main thread.
class MEDIA_EXPORT HlsDataSourceProvider {
 public:
  virtual ~HlsDataSourceProvider() = 0;

  struct ReadStatusTraits {
    enum class Codes : StatusCodeType {
      kError,
      kStopped,
      kAborted,
    };
    static constexpr StatusGroupType Group() {
      return "HlsDataSourceProvider::ReadStatus";
    }
  };

  using ReadStatus = TypedStatus<ReadStatusTraits>;
  using ReadResult = ReadStatus::Or<std::unique_ptr<HlsDataSourceStream>>;
  using ReadCb = base::OnceCallback<void(ReadResult)>;

  // Kicks off a read from the given url and the optional constrained byterange
  // and replies with a stream reference, which can be use to continue reading
  // and to extract already fetched data.
  virtual void ReadFromUrl(GURL uri,
                           absl::optional<hls::types::ByteRange> range,
                           ReadCb callback) = 0;

  // Continues to read from an existing stream.
  virtual void ReadFromExistingStream(
      std::unique_ptr<HlsDataSourceStream> stream,
      ReadCb callback) = 0;

  // Aborts all pending reads and calls `callback` when finished.
  virtual void AbortPendingReads(base::OnceClosure callback) = 0;
};

// A buffer-owning wrapper for an HlsDataSource which can be instructed to
// read an entire data source, or to retrieve it in chunks.
class MEDIA_EXPORT HlsDataSourceStream {
 public:
  // The response to a stream read includes a raw pointer back to the stream
  // which allows accessing the data from a read as well as caching a partially
  // read stream handle for continued downloading.
  using StreamId = base::IdType32<HlsDataSourceStream>;

  // Create a stream where `on_destructed_cb` is used to give notice that this
  // class is being destroyed. This class isn't safe to access from anything
  // except for an ownership-holding smart pointer, as the destruction cb may
  // do work across threads.
  HlsDataSourceStream(StreamId stream_id,
                      base::OnceClosure on_destructed_cb,
                      absl::optional<hls::types::ByteRange> range);
  ~HlsDataSourceStream();

  // Streams use an ID associated with a MultiBufferDataSource without
  // owning it.
  StreamId stream_id() const { return stream_id_; }

  // This is the byte position in the MultiBufferDataSource where new data
  // will be read from. This only ever goes up, because these streams are not
  // rewindable.
  size_t read_position() const { return read_position_; }

  size_t buffer_size() const { return buffer_.size(); }

  absl::optional<size_t> max_read_position() const {
    return max_read_position_;
  }

  const uint8_t* raw_data() const { return buffer_.data(); }

  // Often the network data for HLS consists of plain-text manifest files, so
  // this supports accessing the fetched data as a string view.
  std::string_view AsString() const;

  // Has the stream read all possible data?
  bool CanReadMore() const;

  // Clears the internal buffer of data. Continual reads will refill the buffer
  // and reading without clearing will append to the end of the buffer.
  void Clear();

  // Used by a HlsDataSourceProvider implementation to finish adding data to
  // the internal buffer.
  void UnlockStreamPostWrite(int read_size, bool end_of_stream);

  // Used by a HlsDataSourceProvider implementation to start adding new data,
  // which means ensuring that there is enough space for the expected write, as
  // well as returning the correct buffer address to write into.
  uint8_t* LockStreamForWriting(int ensure_minimum_space);

 private:
  const StreamId stream_id_;

  // Active buffer data. Reading without clearing will append new data
  // to the end of the buffer. Clearing will not reset the read-head, but will
  // empty this buffer.
  // TODO(crbug/1266991): Consider swapping out the vector with a more
  // size-flexible data structure to avoid resizing.
  std::vector<uint8_t> buffer_;

  size_t read_position_ = 0;

  // The write index into `buffer_`. This gets reset on flush.
  size_t write_index_ = 0;

  // If this optional value is set, then data can't be read past this maximum
  // value.
  absl::optional<size_t> max_read_position_;

  // The data source read response indicated that the stream has ended.
  bool reached_end_of_stream_ = false;

  // The stream is unable to start a second write or clear until it is unlocked
  // by UnlockStreamPostWrite.
  bool stream_locked_ = false;

  base::OnceClosure on_destructed_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HlsDataSourceStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
