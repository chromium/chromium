// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"

namespace media {

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::NeedsBitstreamConversion() const {
  return false;
}

bool VideoDecoder::CanReadWithoutStalling() const {
  return true;
}

int VideoDecoder::GetMaxDecodeRequests() const {
  return 1;
}

bool VideoDecoder::FramesHoldExternalResources() const {
  return false;
}

// static
int VideoDecoder::GetRecommendedThreadCount(int desired_threads) {
  // If the thread count is specified on the command line, respect it so long as
  // it's greater than zero.
  const auto threads =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kVideoThreads);
  int decode_threads;
  if (base::StringToInt(threads, &decode_threads) && decode_threads > 0)
    return decode_threads;

  // Clamp to the number of available logical processors/cores.
  desired_threads =
      std::min(desired_threads, base::SysInfo::NumberOfProcessors());

  // Always try to use at least two threads for video decoding. There is little
  // reason not to since current day CPUs tend to be multi-core and we measured
  // performance benefits on older machines such as P4s with hyperthreading.
  //
  // All our software video decoders treat having one thread the same as having
  // zero threads; I.e., decoding will execute on the calling thread. Therefore,
  // at least two threads are required to allow decoding to progress outside of
  // each Decode() call.
  return std::clamp(desired_threads,
                     static_cast<int>(limits::kMinVideoDecodeThreads),
                     static_cast<int>(limits::kMaxVideoDecodeThreads));
}

}  // namespace media
