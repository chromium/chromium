// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/mt21/mt21_decompressor.h"

#include <sched.h>
#include <stdlib.h>

#include "base/bits.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/v4l2/mt21/mt21_util.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace media {

namespace {

template <class T>
void MT21ToMM21(const uint8_t* src,
                const uint8_t* footer,
                uint8_t* dest,
                size_t start_offset,
                size_t width,
                size_t height,
                const GolombRiceTableEntry* symbol_cache) {
  std::vector<T> subblock_bins[2];
  uint8_t scratch[kMT21ScratchMemorySize] __attribute__((aligned(16)));

  for (size_t block_offset = 0; block_offset < width * height;
       block_offset += kMT21BlockSize) {
    BinSubblocks<T>(src, footer, dest, block_offset + start_offset,
                    subblock_bins);
  }

  // Handle high-entropy passthrough subblocks.
  for (T& subblock : subblock_bins[1]) {
    memcpy(subblock.dest, subblock.src, subblock.len);
  }

  // Vector decompress as many blocks as possible.
  size_t i = 0;
  for (; i + kNumOutputLanes - 1 < subblock_bins[0].size();
       i += kNumOutputLanes) {
    VectorDecompressSubblockHelper<T>(subblock_bins[0], i, scratch);
  }
  // Scalar decompress the remainder.
  for (; i < subblock_bins[0].size(); i++) {
    DecompressSubblockHelper<T>(subblock_bins[0][i], symbol_cache);
  }
}

void DecompressAndDetile(const MT21DecompressionJob& job,
                         uint8_t* pivot,
                         const GolombRiceTableEntry* symbol_cache) {
  if (job.is_chroma) {
    MT21ToMM21<MT21UVSubblock>(job.src, job.footer, pivot, job.offset,
                               job.width, job.height, symbol_cache);
  } else {
    MT21ToMM21<MT21YSubblock>(job.src, job.footer, pivot, job.offset, job.width,
                              job.height, symbol_cache);
  }

  libyuv::DetilePlane(pivot + job.offset, job.width, job.dest + job.offset,
                      job.width, job.width, job.height,
                      job.is_chroma ? kMT21TileHeight / 2 : kMT21TileHeight);
}

// MT8173 has 2 Cortex A72s and 2 Cortex A53s
constexpr size_t kNumLittleThreads = 2;
constexpr size_t kNumBigThreads = 2;

void MT21WorkerEntry(cpu_set_t mask,
                     std::atomic_bool& should_shutdown,
                     const GolombRiceTableEntry* symbol_cache,
                     uint8_t* pivot,
                     scoped_refptr<MT21DecompressionJob> job) {
  sched_setaffinity(0, sizeof(cpu_set_t), &mask);

  while (true) {
    job->wakeup_event.Wait();

    if (should_shutdown) {
      break;
    }

    DecompressAndDetile(*job, pivot, symbol_cache);

    job->done_event.Signal();
  }
}

}  // namespace

MT21DecompressionJob::MT21DecompressionJob(const uint8_t* src,
                                           const uint8_t* footer,
                                           size_t offset,
                                           uint8_t* dest,
                                           size_t width,
                                           size_t height,
                                           bool is_chroma)
    : wakeup_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                   base::WaitableEvent::InitialState::NOT_SIGNALED),
      done_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                 base::WaitableEvent::InitialState::NOT_SIGNALED) {
  this->src = src;
  this->footer = footer;
  this->offset = offset;
  this->dest = dest;
  this->width = width;
  this->height = height;
  this->is_chroma = is_chroma;
}

MT21Decompressor::MT21Decompressor(gfx::Size resolution) {
  symbol_cache_ = new GolombRiceTableEntry[kGolombRiceCacheSize];
  PopulateGolombRiceCache(symbol_cache_);

  aligned_resolution_ =
      gfx::Size(base::bits::AlignUp(static_cast<size_t>(resolution.width()),
                                    kMT21TileWidth),
                base::bits::AlignUp(static_cast<size_t>(resolution.height()),
                                    kMT21TileHeight));

  // Big cores are CPUs 2 and 3, while the little cores are 0 and 1.
  cpu_set_t mask;
  CPU_ZERO(&mask);
  for (size_t i = kNumLittleThreads; i < kNumLittleThreads + kNumBigThreads;
       i++) {
    CPU_SET(i, &mask);
  }
  big_core_pivot_ =
      static_cast<uint8_t*>(aligned_alloc(16, aligned_resolution_.GetArea()));
  for (size_t i = 0; i < kNumBigThreads; i++) {
    scoped_refptr<MT21DecompressionJob> job =
        base::MakeRefCounted<MT21DecompressionJob>(nullptr, nullptr, 0, nullptr,
                                                   0, 0, false);
    big_core_jobs_.push_back(job);
    big_core_threads_.emplace_back(MT21WorkerEntry, mask,
                                   std::ref(should_shutdown_), symbol_cache_,
                                   big_core_pivot_, job);
  }

  CPU_ZERO(&mask);
  for (size_t i = 0; i < kNumLittleThreads; i++) {
    CPU_SET(i, &mask);
  }
  little_core_pivot_ =
      static_cast<uint8_t*>(aligned_alloc(16, aligned_resolution_.GetArea()));
  for (size_t i = 0; i < kNumLittleThreads; i++) {
    scoped_refptr<MT21DecompressionJob> job =
        base::MakeRefCounted<MT21DecompressionJob>(nullptr, nullptr, 0, nullptr,
                                                   0, 0, true);
    little_core_jobs_.push_back(job);
    little_core_threads_.emplace_back(MT21WorkerEntry, mask,
                                      std::ref(should_shutdown_), symbol_cache_,
                                      little_core_pivot_, job);
  }

  // Experimental evidence shows that A53s decompress MT21 blocks at about half
  // the speed of A72s. This conveniently means that if split the chroma plane
  // between the A53s and the luma plane between the A72s, we should perfectly
  // balance the load.

  size_t uv_split_height = base::bits::AlignUp(
      static_cast<size_t>(aligned_resolution_.height() / 2 / 2),
      kMT21TileHeight / 2);
  size_t uv_split_offset = uv_split_height * aligned_resolution_.width();
  little_core_jobs_[0]->offset = 0;
  little_core_jobs_[0]->width = aligned_resolution_.width();
  little_core_jobs_[0]->height = uv_split_height;
  little_core_jobs_[1]->offset = uv_split_offset;
  little_core_jobs_[1]->width = aligned_resolution_.width();
  little_core_jobs_[1]->height =
      aligned_resolution_.height() / 2 - uv_split_height;

  size_t y_split_height = base::bits::AlignUp(
      static_cast<size_t>(aligned_resolution_.height() / 2), kMT21TileHeight);
  size_t y_split_offset = y_split_height * aligned_resolution_.width();
  big_core_jobs_[0]->offset = 0;
  big_core_jobs_[0]->width = aligned_resolution_.width();
  big_core_jobs_[0]->height = y_split_height;
  big_core_jobs_[1]->offset = y_split_offset;
  big_core_jobs_[1]->width = aligned_resolution_.width();
  big_core_jobs_[1]->height = aligned_resolution_.height() - y_split_height;
}

MT21Decompressor::~MT21Decompressor() {
  should_shutdown_ = true;
  for (auto& job : little_core_jobs_) {
    job->wakeup_event.Signal();
  }
  for (auto& job : big_core_jobs_) {
    job->wakeup_event.Signal();
  }
  for (size_t i = 0; i < kNumLittleThreads; i++) {
    little_core_threads_[i].join();
  }
  for (size_t i = 0; i < kNumBigThreads; i++) {
    big_core_threads_[i].join();
  }

  delete little_core_pivot_;
  delete big_core_pivot_;

  delete symbol_cache_;
}

void MT21Decompressor::MT21ToNV12(const uint8_t* src_y,
                                  const uint8_t* src_uv,
                                  const size_t y_buf_size,
                                  const size_t uv_buf_size,
                                  uint8_t* dest_y,
                                  uint8_t* dest_uv) {
  const uint8_t* y_footer =
      ComputeFooterOffset(aligned_resolution_.GetArea(), y_buf_size,
                          kMT21YFooterAlignment) +
      src_y;
  const uint8_t* uv_footer =
      ComputeFooterOffset(aligned_resolution_.GetArea() / 2, uv_buf_size,
                          kMT21UVFooterAlignment) +
      src_uv;

  // Start little core jobs.
  for (auto& job : little_core_jobs_) {
    job->src = src_uv;
    job->footer = uv_footer;
    job->dest = dest_uv;
    job->wakeup_event.Signal();
  }

  // Start big core jobs.
  for (auto& job : big_core_jobs_) {
    job->src = src_y;
    job->footer = y_footer;
    job->dest = dest_y;
    job->wakeup_event.Signal();
  }

  // Wait for everything to finish.
  for (auto& job : little_core_jobs_) {
    job->done_event.Wait();
  }
  for (auto& job : big_core_jobs_) {
    job->done_event.Wait();
  }
}

}  // namespace media
