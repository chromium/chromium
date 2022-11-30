// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains declarations for deleters for use with |scoped_ptr|. To
// avoid requiring additional #includes, the (inline) definitions are in
// ffmpeg_common.h. (Forward declarations of deleters aren't sufficient for
// |scoped_ptr|.)

#ifndef MEDIA_FFMPEG_FFMPEG_DELETERS_H_
#define MEDIA_FFMPEG_FFMPEG_DELETERS_H_

namespace media {

// Wraps FFmpeg's av_free() in a class that can be passed as a template argument
// to scoped_ptr_malloc.
struct ScopedPtrAVFree {
  void operator()(void* x) const;
};

// Calls av_packet_free(). Do not use this with an AVPacket instance that was
// allocated with new or manually av_malloc'd. ScopedAVPacket is the
// recommended way to manage an AVPacket's lifetime.
struct ScopedPtrAVFreePacket {
  void operator()(void* x) const;
};

// Frees an AVCodecContext object in a class that can be passed as a Deleter
// argument to scoped_ptr_malloc.
struct ScopedPtrAVFreeContext {
  void operator()(void* x) const;
};

// Frees an AVFrame object in a class that can be passed as a Deleter argument
// to scoped_ptr_malloc.
struct ScopedPtrAVFreeFrame {
  void operator()(void* x) const;
};

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_DELETERS_H_
