// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser_buffer.h"

namespace media {

class DecryptConfig;
class OffsetByteQueue;
class StreamParserBuffer;

namespace mp2t {

class MEDIA_EXPORT EsParser {
 public:
  using EmitBufferCB =
      base::RepeatingCallback<void(scoped_refptr<StreamParserBuffer>)>;
  using GetDecryptConfigCB = base::RepeatingCallback<const DecryptConfig*()>;

  EsParser();

  EsParser(const EsParser&) = delete;
  EsParser& operator=(const EsParser&) = delete;

  virtual ~EsParser();

  // ES parsing.
  // Should use kNoTimestamp when a timestamp is not valid.
  bool Parse(const uint8_t* buf,
             int size,
             base::TimeDelta pts,
             DecodeTimestamp dts);

  // Flush any pending buffer.
  virtual void Flush() = 0;

  // Reset the state of the ES parser.
  void Reset();

 protected:
  struct TimingDesc {
    TimingDesc();
    TimingDesc(DecodeTimestamp dts, base::TimeDelta pts);

    DecodeTimestamp dts;
    base::TimeDelta pts;
  };

  // Parse ES data from |es_queue_|.
  // Return true when successful.
  virtual bool ParseFromEsQueue() = 0;

  // Reset the internal state of the ES parser.
  virtual void ResetInternal() = 0;

  // Get the timing descriptor with the largest byte count that is less or
  // equal to |es_byte_count|.
  // This timing descriptor and all the ones that come before (in stream order)
  // are removed from list |timing_desc_list_|.
  // If no timing descriptor is found, then the default TimingDesc is returned.
  TimingDesc GetTimingDescriptor(int64_t es_byte_count);

  // Bytes of the ES stream that have not been emitted yet.
  std::unique_ptr<media::OffsetByteQueue> es_queue_;

 private:
  // Anchor some timing information into the ES queue.
  // Here are two examples how this timing info is applied according to
  // the MPEG-2 TS spec - ISO/IEC 13818:
  // - "In the case of audio, if a PTS is present in PES packet header it shall
  // refer to the first access unit commencing in the PES packet. An audio
  // access unit commences in a PES packet if the first byte of the audio
  // access unit is present in the PES packet."
  // - "For AVC video streams conforming to one or more profiles defined
  // in Annex A of Rec. ITU-T H.264 | ISO/IEC 14496-10 video, if a PTS is
  // present in the PES packet header, it shall refer to the first AVC access
  // unit that commences in this PES packet.
  std::list<std::pair<int64_t, TimingDesc>> timing_desc_list_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_ES_PARSER_H_
