// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEOTOOLBOX_HELPERS_H_
#define MEDIA_BASE_MAC_VIDEOTOOLBOX_HELPERS_H_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/notreached.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"

namespace media::video_toolbox {

// Copy a H.264/HEVC frame stored in a CM sample buffer to an Annex B buffer.
// Copies parameter sets for keyframes before the frame data as well.
MEDIA_EXPORT bool CopySampleBufferToAnnexBBuffer(VideoCodec codec,
                                                 CMSampleBufferRef sbuf,
                                                 bool keyframe,
                                                 std::string* annexb_buffer);
MEDIA_EXPORT bool CopySampleBufferToAnnexBBuffer(VideoCodec codec,
                                                 CMSampleBufferRef sbuf,
                                                 bool keyframe,
                                                 size_t annexb_buffer_size,
                                                 char* annexb_buffer,
                                                 size_t* used_buffer_size);

struct ScopedVTCompressionSessionRefTraits {
  static VTCompressionSessionRef InvalidValue() { return nullptr; }
  static VTCompressionSessionRef Retain(VTCompressionSessionRef session) {
    NOTREACHED() << "Only compatible with ASSUME policy";
  }
  static void Release(VTCompressionSessionRef session) {
    // Blocks until all pending frames have been flushed out.
    VTCompressionSessionInvalidate(session);
    CFRelease(session);
  }
};

// A scoper for VTCompressionSessionRef that makes sure
// VTCompressionSessionInvalidate() is called before releasing.
using ScopedVTCompressionSessionRef =
    base::apple::ScopedTypeRef<VTCompressionSessionRef,
                               ScopedVTCompressionSessionRefTraits>;

// Helper class to add session properties to a VTCompressionSessionRef.
class MEDIA_EXPORT SessionPropertySetter {
 public:
  SessionPropertySetter(
      base::apple::ScopedCFTypeRef<VTCompressionSessionRef> session);
  ~SessionPropertySetter();

  bool IsSupported(CFStringRef key);
  bool Set(CFStringRef key, int32_t value);
  bool Set(CFStringRef key, bool value);
  bool Set(CFStringRef key, double value);
  bool Set(CFStringRef key, CFStringRef value);
  bool Set(CFStringRef key, CFArrayRef value);

 private:
  base::apple::ScopedCFTypeRef<VTCompressionSessionRef> session_;
  base::apple::ScopedCFTypeRef<CFDictionaryRef> supported_keys_;
};

}  // namespace media::video_toolbox

#endif  // MEDIA_BASE_MAC_VIDEOTOOLBOX_HELPERS_H_
