// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp2t/es_parser.h"

#include "base/logging.h"
#include "media/base/byte_queue.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/common/offset_byte_queue.h"

namespace media {
namespace mp2t {

EsParser::TimingDesc::TimingDesc()
    : dts(kNoDecodeTimestamp), pts(kNoTimestamp) {}

EsParser::TimingDesc::TimingDesc(
    DecodeTimestamp dts_in, base::TimeDelta pts_in)
    : dts(dts_in),
      pts(pts_in) {
}

EsParser::EsParser()
    : es_queue_(new media::OffsetByteQueue()) {
}

EsParser::~EsParser() {
}

bool EsParser::Parse(const uint8_t* buf,
                     int size,
                     base::TimeDelta pts,
                     DecodeTimestamp dts) {
  DCHECK(buf);
  DCHECK_GE(size, 0);
  // TS parser may try to give us zero-size data.
  if (size == 0)
    return false;

  if (pts != kNoTimestamp) {
    // Link the end of the byte queue with the incoming timing descriptor.
    TimingDesc timing_desc(dts, pts);
    timing_desc_list_.push_back(
        std::pair<int64_t, TimingDesc>(es_queue_->tail(), timing_desc));
  }

  // Add the incoming bytes to the ES queue.
  if (!es_queue_->Push(
          base::make_span(buf, base::checked_cast<size_t>(size)))) {
    DVLOG(2) << "Failed to push buf of size " << size;
    return false;
  }

  return ParseFromEsQueue();
}

void EsParser::Reset() {
  es_queue_.reset(new media::OffsetByteQueue());
  timing_desc_list_.clear();
  ResetInternal();
}

EsParser::TimingDesc EsParser::GetTimingDescriptor(int64_t es_byte_count) {
  TimingDesc timing_desc;
  while (!timing_desc_list_.empty() &&
         timing_desc_list_.front().first <= es_byte_count) {
    timing_desc = timing_desc_list_.front().second;
    timing_desc_list_.pop_front();
  }
  return timing_desc;
}

}  // namespace mp2t
}  // namespace media
