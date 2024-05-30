// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_H_
#define MEDIA_BASE_STREAM_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/media_export.h"

namespace media {

class MediaLog;
class MediaTracks;
class StreamParserBuffer;

// Abstract interface for parsing media byte streams.
class MEDIA_EXPORT StreamParser {
 public:
  using BufferQueue = base::circular_deque<scoped_refptr<StreamParserBuffer>>;

  // Range of |TrackId| is dependent upon stream parsers.
  // It is the key for BufferQueueMap structure returned by stream parsers.
  using TrackId = int;

  // Map of track ID to decode-timestamp-ordered buffers for the track.
  using BufferQueueMap = base::flat_map<TrackId, BufferQueue>;

  // With incremental parsing, parse can succeed yet also still indicate there
  // is more uninspected data to parse. This enum identifies the possible
  // results of incremental parsing by a StreamParser.
  // TODO:(crbug.com/1378678): Investigate usage of TypeStatus<T>::Or<U> where
  // this is used.
  enum class ParseStatus {
    kFailed,
    kSuccess,
    kSuccessHasMoreData,
  };

  // Incremental parse of a potentially large pending data considers up to this
  // many further bytes from the pending bytes during the Parse() call. Moved
  // from older incremental append+parse logic in SourceBuffer, this size value
  // was originally chosen as 128KiB to not block the renderer event loop very
  // long. This value had been selected by looking at YouTube SourceBuffer usage
  // across a variety of bitrates, to allow relatively large appendBuffer()
  // calls while keeping each parse iteration's duration within ~5-15ms range.
  // This value may change in future updates as platform capabilities have
  // generally improved.
  // TODO(crbug.com/40244251): Tune this experimentally.
  static constexpr int kMaxPendingBytesPerParse = 128 * 1024;  // 128KiB

  // Stream parameters passed in InitCB.
  struct MEDIA_EXPORT InitParameters {
    InitParameters(base::TimeDelta duration);

    // Stream duration.
    base::TimeDelta duration;

    // Indicates the source time associated with presentation timestamp 0. A
    // null Time is returned if no mapping to Time exists.
    base::Time timeline_offset;

    // Indicates live stream.
    StreamLiveness liveness = StreamLiveness::kUnknown;

    // Counts of tracks detected by type within this stream. Not all of these
    // tracks may be selected for use by the parser.
    int detected_audio_track_count = 0;
    int detected_video_track_count = 0;
  };

  // Indicates completion of parser initialization.
  //   params - Stream parameters.
  typedef base::OnceCallback<void(const InitParameters& params)> InitCB;

  // Indicates when new stream configurations have been parsed.
  // First parameter - An object containing information about media tracks as
  //                   well as audio/video decoder configs associated with each
  //                   track the parser will use from the stream.
  // Return value - True if the new configurations are accepted.
  //                False if the new configurations are not supported
  //                and indicates that a parsing error should be signalled.
  typedef base::RepeatingCallback<bool(std::unique_ptr<MediaTracks>)>
      NewConfigCB;

  // New stream buffers have been parsed.
  // First parameter - A map of track ids to queues of newly parsed buffers.
  // Return value - True indicates that the buffers are accepted.
  //                False if something was wrong with the buffers and a parsing
  //                error should be signalled.
  typedef base::RepeatingCallback<bool(const BufferQueueMap&)> NewBuffersCB;

  // Signals the beginning of a new media segment.
  typedef base::RepeatingCallback<void()> NewMediaSegmentCB;

  // Signals the end of a media segment.
  typedef base::RepeatingCallback<void()> EndMediaSegmentCB;

  // A new potentially encrypted stream has been parsed.
  // First parameter - The type of the initialization data associated with the
  //                   stream.
  // Second parameter - The initialization data associated with the stream.
  typedef base::RepeatingCallback<void(EmeInitDataType,
                                       const std::vector<uint8_t>&)>
      EncryptedMediaInitDataCB;

  StreamParser();

  StreamParser(const StreamParser&) = delete;
  StreamParser& operator=(const StreamParser&) = delete;

  virtual ~StreamParser();

  // Initializes the parser with necessary callbacks. Must be called before any
  // data is passed to AppendToParseBuffer() or any call to Parse() or
  // ProcessChunks(). `init_cb` will be called once enough data has been parsed
  // to determine the initial stream configurations, presentation start time,
  // and duration.
  virtual void Init(InitCB init_cb,
                    NewConfigCB config_cb,
                    NewBuffersCB new_buffers_cb,
                    EncryptedMediaInitDataCB encrypted_media_init_data_cb,
                    NewMediaSegmentCB new_segment_cb,
                    EndMediaSegmentCB end_of_segment_cb,
                    MediaLog* media_log) = 0;

  // Called during the reset parser state algorithm. This flushes the current
  // parser and puts the parser in a state where it can receive data. This
  // method does not need to invoke the EndMediaSegmentCB since the parser reset
  // algorithm already resets the segment parsing state.
  virtual void Flush() = 0;

  // Returns the MSE byte stream format registry's "Generate Timestamps Flag"
  // for the byte stream corresponding to this parser.
  virtual bool GetGenerateTimestampsFlag() const = 0;

  // Called when there is new data to parse. Any previously provided data must
  // have been fully attempted to be parsed (by one or more calls to Parse()
  // until kSuccess results) or has been Flush()'ed. Returns true if the parser
  // successfully copied the data from `buf` for use in future Parse() calls.
  // Returns false if the parser was unable to allocate resources; content in
  // `buf` is not copied as a result, and this failure is reported (through
  // various layers) up to the SourceBuffer's implementation of appendBuffer(),
  // which should then notify the app of append failure using a
  // `QuotaExceededErr` exception per the MSE specification. App could use a
  // back-off and retry strategy or otherwise alter their behavior to attempt to
  // buffer media for further playback.
  [[nodiscard]] virtual bool AppendToParseBuffer(
      base::span<const uint8_t> buf) = 0;

  // Attempts to parse more data previously provided via AppendToParseBuffer().
  // May not attempt to parse all of it in one pass;
  // `max_pending_bytes_to_inspect` should normally be set to
  // kMaxPendingBytesPerParse, except if the caller needs to use a different
  // amount (for example, a file verification parse or a test case that needs to
  // involve a larger or smaller amount of the pending data in one call to
  // Parse()).
  // Returns kSuccess if the parse succeeded and all previously provided data
  // from AppendToParseBuffer() has been inspected.
  // Returns kSuccessHasMoreData if the parse succeeded, yet there remains
  // uninspected data remaining from AppendToParseBuffer(); more call(s) to this
  // method are necessary for the parser to attempt inspection of that data.
  // Returns kFailed if there was a parse error.
  //
  // Regular "bytestream-formatted" StreamParsers should fully implement
  // Parse(), but WebCodecsEncodedChunkStreamParsers should instead fully
  // implement ProcessChunks().
  [[nodiscard]] virtual ParseStatus Parse(int max_pending_bytes_to_inspect) = 0;
  [[nodiscard]] virtual bool ProcessChunks(
      std::unique_ptr<BufferQueue> buffer_queue);
};

// Appends to |merged_buffers| the provided buffers in decode-timestamp order.
// Any previous contents of |merged_buffers| is assumed to have lower
// decode timestamps versus the provided buffers. All provided buffer queues
// are assumed to already be in decode-timestamp order.
// Returns false if any of the provided audio/video buffers are found
// to not be in decode timestamp order, or have a decode timestamp less than
// the last buffer, if any, in |merged_buffers|. Partial results may exist
// in |merged_buffers| in this case. Returns true on success.
// No validation of media type within the various buffer queues is done here.
// TODO(wolenetz/acolwell): Merge incrementally in parsers to eliminate
// subtle issues with tie-breaking. See http://crbug.com/338484.
MEDIA_EXPORT bool MergeBufferQueues(const StreamParser::BufferQueueMap& buffers,
                                    StreamParser::BufferQueue* merged_buffers);

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_H_
