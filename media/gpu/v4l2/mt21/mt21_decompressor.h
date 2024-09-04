// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Decompression utility for the MT21C pixel format.
//
// Note that this file and its corresponding .cc file have some very SoC
// specific code. While we would ideally like to avoid tying code so closely
// with a specific chip, this code is in the critical path for video decoding,
// and we know that we will only ever need to run this code on the MT8173. Every
// other SoC in the MT81XX line support a pixel format called MM21, which we
// have generic support for in libyuv.
//
// We may some day decide to try using MT21C on other chips in the MT81XX line,
// but we will need to change significant sections of this code to make that
// viable. Our assumptions about the relative speed the big and little cores,
// the number of cores, the CPU IDs of the cores, the timings of the SIMD
// instructions, the availability of ARM64, etc, will all be incorrect.

#ifndef MEDIA_GPU_V4L2_MT21_MT21_DECOMPRESSOR_H_
#define MEDIA_GPU_V4L2_MT21_MT21_DECOMPRESSOR_H_

#include "build/build_config.h"

#if !defined(ARCH_CPU_ARM_FAMILY)
#error "MT21Decompressor is only intended to run on MT8173 (ARM)"
#endif

#if !(defined(COMPILER_GCC) || defined(__clang__))
#error "MT21Decompressor is only intended to be built with GCC or Clang"
#endif

#include <stdint.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct GolombRiceTableEntry;

struct MT21DecompressionJob : public base::RefCounted<MT21DecompressionJob> {
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  MT21DecompressionJob(const uint8_t* src,
                       const uint8_t* footer,
                       size_t offset,
                       uint8_t* dest,
                       size_t width,
                       size_t height,
                       bool is_chroma);
  const uint8_t* src;
  const uint8_t* footer;
  size_t offset;
  RAW_PTR_EXCLUSION uint8_t* dest;
  size_t width;
  size_t height;
  bool is_chroma;
  base::WaitableEvent wakeup_event;
  base::WaitableEvent done_event;

 private:
  friend class base::RefCounted<MT21DecompressionJob>;
  ~MT21DecompressionJob() = default;
};

// We considered making this an ImageProcessorBackend, but it turns out we need
// access to the raw V4L2 buffer. MT21C planes have a "secret" footer containing
// metadata necessary for decompression appended to the beginning of the last
// page in the buffer. This extra data is totally unknown to Chrome abstractions
// like VideoFrame, which just assume a plane's size is determined by stride and
// height.
class MT21Decompressor {
 public:
  MT21Decompressor(gfx::Size resolution);
  ~MT21Decompressor();

  void MT21ToNV12(const uint8_t* src_y,
                  const uint8_t* src_uv,
                  const size_t y_buf_size,
                  const size_t uv_buf_size,
                  uint8_t* dest_y,
                  uint8_t* dest_uv);

 private:
  // We divide the frame horizontally 4 times and distribute the job among
  // the 4 CPU cores in the MT8173. Two of these cores are little cores, so we
  // want to divide the task unevenly and make sure the smaller 2 tasks end up
  // scheduled on the smaller cores. In order to accomplish this, we circumvent
  // Chrome's threading system entirely and use raw operating system threads, so
  // we can use sched_setaffinity().
  //
  // One alternative that was considered was breaking the decompression up into
  // a bunch of little atomic tasks and using a threadpool, and just letting
  // the OS scheduler figure out the division of labor. This approach has the
  // significant drawback however of not only introducing more overhead, but
  // more importantly, having potentially very poor memory locality.
  //
  // Note that we also keep threads alive and waiting between runs of the
  // decompression routine. Experimental evidence has indicated that the
  // overhead of start and join syscalls substantially lengthen decompression
  // times, so we just use userspace semaphores for synchronization instead.
  std::atomic_bool should_shutdown_ = false;
  std::vector<std::thread> big_core_threads_;
  std::vector<scoped_refptr<MT21DecompressionJob>> big_core_jobs_;
  raw_ptr<uint8_t> big_core_pivot_;
  std::vector<std::thread> little_core_threads_;
  std::vector<scoped_refptr<MT21DecompressionJob>> little_core_jobs_;
  raw_ptr<uint8_t> little_core_pivot_;

  gfx::Size aligned_resolution_;

  raw_ptr<GolombRiceTableEntry> symbol_cache_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_MT21_MT21_DECOMPRESSOR_H_
