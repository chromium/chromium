// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
#define MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/types/id_type.h"
#include "media/base/media_export.h"
#include "media/base/status.h"
#include "media/formats/hls/types.h"
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

  // Represents reading from a specific URI at the given byte range. Multiple
  // segments can be added to a read queue to join chunks together from either
  // multiple URIs or from multiple disjoing ranges on the same URI.
  struct UrlDataSegment {
    const GURL uri;
    const std::optional<hls::types::ByteRange> range;
    const bool bypass_cache;
  };
  using SegmentQueue = base::queue<UrlDataSegment>;

  // Kicks off a read to a chain of segments, and replies with a stream
  // reference which can be used to continue fetching partial data.
  virtual void ReadFromCombinedUrlQueue(SegmentQueue segments,
                                        ReadCb callback) = 0;

  // Continues to read from an existing stream.
  virtual void ReadFromExistingStream(
      std::unique_ptr<HlsDataSourceStream> stream,
      ReadCb callback) = 0;

  // Aborts all pending reads and calls `callback` when finished.
  virtual void AbortPendingReads(base::OnceClosure callback) = 0;

  // Helper function for reading from a single segment by creating a queue of
  // size 1 for use with `ReadFromCombinedUrlQueue`
  void ReadFromUrl(UrlDataSegment segment, ReadCb callback);
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
                      HlsDataSourceProvider::SegmentQueue segments,
                      base::OnceClosure on_destructed_cb);
  ~HlsDataSourceStream();

  // Streams use an ID associated with a MultiBufferDataSource without
  // owning it.
  StreamId stream_id() const { return stream_id_; }

  // This is the byte position in the MultiBufferDataSource where new data
  // will be read from. This only ever goes up, because these streams are not
  // rewindable.
  size_t read_position() const { return read_position_; }

  size_t buffer_size() const { return buffer_.size(); }

  std::optional<size_t> max_read_position() const { return max_read_position_; }

  const uint8_t* raw_data() const { return buffer_.data(); }

  uint64_t memory_usage() const { return memory_usage_; }

  bool would_taint_origin() const { return would_taint_origin_; }

  // Allows the stream creator to update memory usage after the first or after
  // subsequent reads.
  void set_total_memory_usage(uint64_t usage) { memory_usage_ = usage; }

  // A stream's origin is considered tainted if any backing data source involved
  // in this playback is tainted.
  void set_would_taint_origin() { would_taint_origin_ = true; }

  // Often the network data for HLS consists of plain-text manifest files, so
  // this supports accessing the fetched data as a string view.
  std::string_view AsString() const;

  // Determines whether the current segment has finished reading, and there are
  // more segments in the queue to read from.
  bool RequiresNextDataSource() const;

  // Gets the next segment URI from the queue of segments. It is invalid to call
  // this method if `RequiresNextDataSource` does not return true. This
  // method will also update the internal range if the segment has one.
  GURL GetNextSegmentURI();

  // Gets the next segment URI and its cache bypass option from the queue of
  // segments. It is invalid to call this method if `RequiresNextDataSource`
  // does not return true. This method will also update the internal range if
  // the segment has one.
  std::pair<GURL, bool> GetNextSegmentURIAndCacheStatus();

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
  // TODO(crbug.com/40057824): Consider swapping out the vector with a more
  // size-flexible data structure to avoid resizing.
  std::vector<uint8_t> buffer_;

  // This is critical to security. Once set to true, it must _never_ be set back
  // to false.
  bool would_taint_origin_ = false;

  // The memory usage represents the total memory usage for _all_ streams used
  // in this playback.
  uint64_t memory_usage_ = 0;

  size_t read_position_ = 0;

  // The write index into `buffer_`. This gets reset on flush.
  size_t write_index_ = 0;

  // If this optional value is set, then data can't be read past this maximum
  // value.
  std::optional<size_t> max_read_position_;

  // The data source read response indicated that the stream has ended.
  bool reached_end_of_stream_ = false;

  // The stream is unable to start a second write or clear until it is unlocked
  // by UnlockStreamPostWrite.
  bool stream_locked_ = false;

  // The queue of segments to read from.
  HlsDataSourceProvider::SegmentQueue segments_;

  // Does this stream require a reset to get the next data source.
  bool requires_next_data_source_;

  base::OnceClosure on_destructed_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<HlsDataSourceStream> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_HLS_DATA_SOURCE_PROVIDER_H_
