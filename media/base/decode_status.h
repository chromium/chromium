// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODE_STATUS_H_
#define MEDIA_BASE_DECODE_STATUS_H_

#include <iosfwd>

#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"

namespace media {

enum class DecodeStatus {
  OK = 0,        // Everything went as planned.
  ABORTED,       // Read aborted due to Reset() during pending read.
  DECODE_ERROR,  // Decoder returned decode error. Note: Prefixed by DECODE_
                 // since ERROR is a reserved name (special macro) on Windows.
  DECODE_STATUS_MAX = DECODE_ERROR
};

MEDIA_EXPORT const char* GetDecodeStatusString(DecodeStatus status);

// Helper function so that DecodeStatus can be printed easily.
MEDIA_EXPORT std::ostream& operator<<(std::ostream& os,
                                      const DecodeStatus& status);

// Helper class for ensuring that Decode() traces are properly unique and closed
// if the Decode is aborted via a WeakPtr invalidation. We use the |this|
// pointer of the ScopedDecodeTrace object itself as the id. Since the callback
// owns the class it's guaranteed to be unique.
class MEDIA_EXPORT ScopedDecodeTrace {
 public:
  // Returns true if tracing is enabled for the media category. If false,
  // clients should avoid creating ScopedDecodeTrace objects.
  static bool IsEnabled();

  // Begins an asynchronous trace with the given name and properties. Providing
  // the DecoderBuffer itself yields the most information in the trace.
  ScopedDecodeTrace(const char* trace_name, const DecoderBuffer& buffer);
  ScopedDecodeTrace(const char* trace_name,
                    bool is_key_frame,
                    base::TimeDelta timestamp);
  ~ScopedDecodeTrace();

  // Completes the Decode() trace with the given status.
  void EndTrace(DecodeStatus status);

 private:
  const char* trace_name_;
  bool closed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedDecodeTrace);
};

}  // namespace media

#endif  // MEDIA_BASE_DECODE_STATUS_H_
