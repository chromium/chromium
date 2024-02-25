// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DEMUXER_STREAM_H_
#define MEDIA_BASE_DEMUXER_STREAM_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"
#include "media/base/video_transformation.h"

namespace media {

class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;

enum class StreamLiveness {
  kUnknown,
  kRecorded,
  kLive,
  kMaxValue = kLive,
};

MEDIA_EXPORT std::string GetStreamLivenessName(StreamLiveness liveness);

class MEDIA_EXPORT DemuxerStream {
 public:
  enum Type {
    UNKNOWN,
    AUDIO,
    VIDEO,
    TYPE_MAX = VIDEO,
  };

  // Returns a string representation of |type|.
  static const char* GetTypeName(Type type);

  // Status returned in the Read() callback.
  //  kOk : Indicates the second parameter is Non-NULL and contains media data
  //        or the end of the stream.
  //  kAborted : Indicates an aborted Read(). This can happen if the
  //             DemuxerStream gets flushed and doesn't have any more data to
  //             return. The second parameter MUST be NULL when this status is
  //             returned.
  //  kConfigChange : Indicates that the AudioDecoderConfig or
  //                  VideoDecoderConfig for the stream has changed.
  //                  The DemuxerStream expects an audio_decoder_config() or
  //                  video_decoder_config() call before Read() will start
  //                  returning DecoderBuffers again. The decoder will need this
  //                  new configuration to properly decode the buffers read
  //                  from this point forward. The second parameter MUST be NULL
  //                  when this status is returned.
  //                  This will only be returned if SupportsConfigChanges()
  //                  returns 'true' for this DemuxerStream.
  // kError : Unexpected fatal error happened. Playback should fail.
  enum Status {
    kOk,
    kAborted,
    kConfigChanged,
    kError,
    kStatusMax = kError,
  };

  static const char* GetStatusName(Status status);

  using DecoderBufferVector = std::vector<scoped_refptr<DecoderBuffer>>;
  using ReadCB = base::OnceCallback<void(Status, DecoderBufferVector)>;

  // Request buffers to be returned via the provided callback.
  // The first parameter indicates the status of the read request.
  // If the status is kAborted, kConfigChanged or kError, the vector must be
  // empty. If the status is kOk, the size of vector should be 1<=n<=N, where
  // N is the requested count. The last buffer of the vector could be EOS.
  virtual void Read(uint32_t count, ReadCB read_cb) = 0;

  // Returns the audio/video decoder configuration. It is an error to call the
  // audio method on a video stream and vice versa. After |kConfigChanged| is
  // returned in a Read(), the caller should call this method again to retrieve
  // the new config.
  virtual AudioDecoderConfig audio_decoder_config() = 0;
  virtual VideoDecoderConfig video_decoder_config() = 0;

  // Returns the type of stream.
  virtual Type type() const = 0;

  // Returns liveness of the streams provided, i.e. whether recorded or live.
  virtual StreamLiveness liveness() const;

  virtual void EnableBitstreamConverter();

  // Whether or not this DemuxerStream allows midstream configuration changes.
  //
  // A DemuxerStream that returns 'true' to this may return the 'kConfigChange'
  // status from a Read() call. In this case the client is expected to be
  // capable of taking appropriate action to handle config changes. Otherwise
  // audio_decoder_config() and video_decoder_config()'s return values are
  // guaranteed to remain constant, and the client may make optimizations based
  // on this.
  virtual bool SupportsConfigChanges() = 0;

 protected:
  // Only allow concrete implementations to get deleted.
  virtual ~DemuxerStream();
};

}  // namespace media

#endif  // MEDIA_BASE_DEMUXER_STREAM_H_
