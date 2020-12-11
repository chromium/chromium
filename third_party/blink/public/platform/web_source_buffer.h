/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SOURCE_BUFFER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SOURCE_BUFFER_H_

#include "media/base/stream_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_time_range.h"

namespace blink {

class WebSourceBufferClient;

// Interface for actuating the media engine implementation of Media Source
// extension's SourceBuffer. See also the mediasource module in Blink, and the
// WebSourceBufferClient interface.
class WebSourceBuffer {
 public:
  enum AppendMode { kAppendModeSegments, kAppendModeSequence };

  virtual ~WebSourceBuffer() = default;

  // This will only be called once and only with a non-null pointer to a
  // client whose ownership is not transferred to this WebSourceBuffer.
  virtual void SetClient(WebSourceBufferClient*) = 0;

  virtual bool SetMode(AppendMode) = 0;
  virtual bool GetGenerateTimestampsFlag() = 0;
  virtual WebTimeRanges Buffered() = 0;

  // Returns the highest buffered presentation timestamp of all buffered coded
  // frames, or 0 if nothing is buffered.
  virtual double HighestPresentationTimestamp() = 0;

  // Run coded frame eviction/garbage collection algorithm.
  // |current_playback_time| is HTMLMediaElement::currentTime. The algorithm
  // will try to preserve data around current playback position.
  // |new_data_size| is size of new data about to be appended to SourceBuffer.
  // Could be zero for appendStream if stream size is unknown in advance.
  // Returns false if buffer is still full after eviction.
  virtual bool EvictCodedFrames(double current_playback_time,
                                size_t new_data_size) = 0;

  // Appends data and runs the segment parser loop algorithm (or more simply
  // appends and processes caller-provided media::StreamParserBuffers in the
  // AppendChunks version). The algorithm and associated frame processing may
  // update |*timestamp_offset| if |timestamp_offset| is not null.
  // Returns true on success, otherwise the append error algorithm needs to
  // run with the decode error parameter set to true.
  virtual bool Append(const unsigned char* data,
                      unsigned length,
                      double* timestamp_offset) = 0;
  virtual bool AppendChunks(
      std::unique_ptr<media::StreamParser::BufferQueue> buffer_queue,
      double* timestamp_offset) = 0;

  virtual void ResetParserState() = 0;
  virtual void Remove(double start, double end) = 0;

  // Returns true iff this SourceBuffer supports changing bytestream and codecs
  // to |content_type| and |codecs|.  |content_type| is the ContentType string
  // of the bytestream's MIME type, and |codecs| contains the "codecs" parameter
  // string, if any, of the bytestream's MIME type.
  virtual bool CanChangeType(const WebString& content_type,
                             const WebString& codecs) = 0;

  // Updates this SourceBuffer to parse bytestream and codecs |content_type| and
  // |codecs|. Caller must first ensure CanChangeType() returns true for the
  // same parameters, and must call ResetParserState() prior to calling this, to
  // flush any pending frames. See CanChangeType() for description of
  // |content_type| and |codecs| parameters.
  virtual void ChangeType(const WebString& content_type,
                          const WebString& codecs) = 0;

  virtual bool SetTimestampOffset(double) = 0;

  // Set presentation timestamp for the start of append window.
  virtual void SetAppendWindowStart(double) = 0;

  // Set presentation timestamp for the end of append window.
  virtual void SetAppendWindowEnd(double) = 0;

  // After this method is called, this WebSourceBuffer should never use the
  // client pointer passed to SetClient().
  virtual void RemovedFromMediaSource() = 0;
};

}  // namespace blink

#endif
