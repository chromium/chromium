// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_VIDEOTOOLBOX_HELPERS_H_
#define MEDIA_BASE_MAC_VIDEOTOOLBOX_HELPERS_H_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include "base/apple/scoped_cftyperef.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"

namespace media {

namespace video_toolbox {

// Create a CFDictionaryRef with the given keys and values.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CFDictionaryRef>
DictionaryWithKeysAndValues(CFTypeRef* keys, CFTypeRef* values, size_t size);

// Create a CFDictionaryRef with the given key and value.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CFDictionaryRef>
DictionaryWithKeyValue(CFTypeRef key, CFTypeRef value);

// Create a CFArrayRef with the given array of integers.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CFArrayRef> ArrayWithIntegers(
    const int* v,
    size_t size);

// Create a CFArrayRef with the given int and float values.
MEDIA_EXPORT base::apple::ScopedCFTypeRef<CFArrayRef> ArrayWithIntegerAndFloat(
    int int_val,
    float float_val);

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

}  // namespace video_toolbox

}  // namespace media

#endif  // MEDIA_BASE_MAC_VIDEOTOOLBOX_HELPERS_H_
