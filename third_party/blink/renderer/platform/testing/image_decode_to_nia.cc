// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This program converts an image from stdin (e.g. a JPEG, PNG, etc.) to stdout
// (in the NIA/NIE format, a trivial image file format).
//
// The NIA/NIE file format specification is at:
// https://github.com/google/wuffs/blob/master/doc/spec/nie-spec.md
//
// Pass "-1" or "-first-frame-only" as a command line flag to output NIE (a
// still image) instead of NIA (an animated image). The output format (NIA or
// NIE) depends only on this flag's absence or presence, not on the stdin
// image's format.
//
// There are multiple codec implementations of any given image format. For
// example, as of May 2020, Chromium, Skia and Wuffs each have their own BMP
// decoder implementation. There is no standard "libbmp" that they all share.
// Comparing this program's output (or hashed output) to similar programs in
// other repositories can identify image inputs for which these decoders (or
// different versions of the same decoder) produce different output (pixels).
//
// An equivalent program (using the Skia image codecs) is at:
// https://skia-review.googlesource.com/c/skia/+/290618
//
// An equivalent program (using the Wuffs image codecs) is at:
// https://github.com/google/wuffs/blob/master/example/convert-to-nia/convert-to-nia.c

#include <iostream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/task/single_thread_task_executor.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/skia/include/core/SkColor.h"

static inline void set_u32le(uint8_t* ptr, uint32_t val) {
  ptr[0] = val >> 0;
  ptr[1] = val >> 8;
  ptr[2] = val >> 16;
  ptr[3] = val >> 24;
}

static inline void set_u64le(uint8_t* ptr, uint64_t val) {
  ptr[0] = val >> 0;
  ptr[1] = val >> 8;
  ptr[2] = val >> 16;
  ptr[3] = val >> 24;
  ptr[4] = val >> 32;
  ptr[5] = val >> 40;
  ptr[6] = val >> 48;
  ptr[7] = val >> 56;
}

void write_nix_header(uint32_t magic_u32le, uint32_t width, uint32_t height) {
  uint8_t data[16];
  set_u32le(data + 0, magic_u32le);
  set_u32le(data + 4, 0x346E62FF);  // 4 bytes per pixel non-premul BGRA.
  set_u32le(data + 8, width);
  set_u32le(data + 12, height);
  fwrite(data, 1, 16, stdout);
}

bool write_nia_duration(uint64_t total_duration_micros) {
  // Flicks are NIA's unit of time. One flick (frame-tick) is 1 / 705_600_000
  // of a second. See https://github.com/OculusVR/Flicks
  static constexpr uint64_t flicks_per_ten_micros = 7056;
  uint64_t d = total_duration_micros / 10;
  if (d > (INT64_MAX / flicks_per_ten_micros)) {
    // Converting from micros to flicks would overflow.
    return false;
  }
  d *= flicks_per_ten_micros;

  uint8_t data[8];
  set_u64le(data + 0, d);
  fwrite(data, 1, 8, stdout);
  return true;
}

void write_nie_pixels(uint32_t width,
                      uint32_t height,
                      blink::ImageFrame* frame) {
  static constexpr size_t kBufferSize = 4096;
  uint8_t buf[kBufferSize];
  size_t n = 0;
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      uint32_t pix = *(frame->GetAddr(x, y));
      buf[n++] = pix >> SK_B32_SHIFT;
      buf[n++] = pix >> SK_G32_SHIFT;
      buf[n++] = pix >> SK_R32_SHIFT;
      buf[n++] = pix >> SK_A32_SHIFT;
      if (n == kBufferSize) {
        fwrite(buf, 1, n, stdout);
        n = 0;
      }
    }
  }
  if (n > 0) {
    fwrite(buf, 1, n, stdout);
  }
}

void write_nia_padding(uint32_t width, uint32_t height) {
  // 4 bytes of padding when the width and height are both odd.
  if (width & height & 1) {
    uint8_t data[4];
    set_u32le(data + 0, 0);
    fwrite(data, 1, 4, stdout);
  }
}

void write_nia_footer(int repetition_count, size_t frame_count) {
  // For still (non-animated) images, the number of animation loops has no
  // practical effect: the pixels on screen do not change over time regardless
  // of its value. In the wire format encoding, there might be no explicit
  // "number of animation loops" value listed in the source bytes. Various
  // codec implementations may therefore choose an implicit default of 0 ("loop
  // forever") or 1 ("loop exactly once"). Either is equally valid.
  //
  // However, when comparing the output of this convert-to-NIA program (backed
  // by Chromium's image codecs) with other convert-to-NIA programs, it is
  // useful to canonicalize still images' "number of animation loops" to 0.
  bool override_num_animation_loops = frame_count <= 1;

  uint8_t data[8];
  // kAnimationNone means a still image.
  if (override_num_animation_loops ||
      (repetition_count == blink::kAnimationNone) ||
      (repetition_count == blink::kAnimationLoopInfinite)) {
    set_u32le(data + 0, 0);
  } else {
    // NIA's loop count and Chromium/Skia's repetition count differ by one. See
    // https://github.com/google/wuffs/blob/master/doc/spec/nie-spec.md#nii-footer
    set_u32le(data + 0, 1 + repetition_count);
  }
  set_u32le(data + 4, 0x80000000);
  fwrite(data, 1, 8, stdout);
}

int main(int argc, char* argv[]) {
  base::SingleThreadTaskExecutor main_task_executor;
  base::CommandLine::Init(argc, argv);
  std::unique_ptr<blink::Platform> platform =
      std::make_unique<blink::Platform>();
  blink::Platform::CreateMainThreadAndInitialize(platform.get());

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool first_frame_only = command_line->HasSwitch("1") ||
                          command_line->HasSwitch("first-frame-only");

  std::string src;
  if (!base::ReadStreamToString(stdin, &src)) {
    std::cerr << "could not read stdin\n";
    return 1;
  }
  static constexpr bool data_complete = true;
  std::unique_ptr<blink::ImageDecoder> decoder = blink::ImageDecoder::Create(
      WTF::SharedBuffer::Create(src.data(), src.size()), data_complete,
      blink::ImageDecoder::kAlphaNotPremultiplied,
      blink::ImageDecoder::kDefaultBitDepth, blink::ColorBehavior::kIgnore,
      cc::AuxImage::kDefault, blink::Platform::GetMaxDecodedImageBytes());

  const size_t frame_count = decoder->FrameCount();
  if (frame_count == 0) {
    std::cerr << "no frames\n";
    return 1;
  }

  int image_width;
  int image_height;
  uint64_t total_duration_micros = 0;
  for (size_t i = 0; i < frame_count; i++) {
    blink::ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(i);
    if (!frame) {
      std::cerr << "could not decode frame #" << i << "\n";
      return 1;
    }
    if (frame->GetPixelFormat() != blink::ImageFrame::kN32) {
      std::cerr << "unsupported pixel format\n";
      return 1;
    }
    const int frame_width = decoder->Size().width();
    const int frame_height = decoder->Size().height();
    if ((frame_width < 0) || (frame_height < 0)) {
      std::cerr << "negative dimension\n";
      return 1;
    }
    int64_t duration_micros = decoder->FrameDurationAtIndex(i).InMicroseconds();
    if (duration_micros < 0) {
      std::cerr << "negative animation duration\n";
      return 1;
    }
    total_duration_micros += static_cast<uint64_t>(duration_micros);
    if (total_duration_micros > INT64_MAX) {
      std::cerr << "unsupported animation duration\n";
      return 1;
    }

    if (!first_frame_only) {
      if (i == 0) {
        image_width = frame_width;
        image_height = frame_height;
        write_nix_header(0x41AFC36E,  // "nïA" magic string as a u32le.
                         frame_width, frame_height);
      } else if ((image_width != frame_width) ||
                 (image_height != frame_height)) {
        std::cerr << "non-constant animation dimensions\n";
        return 1;
      }

      if (!write_nia_duration(total_duration_micros)) {
        std::cerr << "unsupported animation duration\n";
        return 1;
      }
    }

    write_nix_header(0x45AFC36E,  // "nïE" magic string as a u32le.
                     frame_width, frame_height);
    write_nie_pixels(frame_width, frame_height, frame);
    if (first_frame_only) {
      return 0;
    }
    write_nia_padding(frame_width, frame_height);
  }
  write_nia_footer(decoder->RepetitionCount(), frame_count);
  return 0;
}
