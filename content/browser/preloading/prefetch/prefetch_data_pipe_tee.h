// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DATA_PIPE_TEE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DATA_PIPE_TEE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace content {

// `PrefetchDataPipeTee` duplicates the `source_` data pipe into multiple cloned
// data pipes.
// `PrefetchDataPipeTee` is kept alive by the second part of `ProducerPair`
// until all cloned data pipes are closed.
//
// To limit the buffer size and fallback gracefully for large data, the number
// of cloned data pipes is limited:
// - Up to 1 while the `source_` data pipe is being read, and
//   (not counting closed pipes)
// - Up to 1 once the data size exceeds the limit
//   (counting closed pipes after that)
// See the comment at `State` below for details.
//
// This limitation should be OK for the purpose of Unified Prefetch Cache Phase
// 1, because
// - the first cloned data pipe is for prerendering,
// - the second cloned data pipe is for main navigation, and
// - at the time of the main navigation starts, the first cloned data pipe is
//   already closed.
class CONTENT_EXPORT PrefetchDataPipeTee final
    : public base::RefCounted<PrefetchDataPipeTee> {
 public:
  explicit PrefetchDataPipeTee(mojo::ScopedDataPipeConsumerHandle source,
                               size_t buffer_limit);

  PrefetchDataPipeTee(const PrefetchDataPipeTee&) = delete;
  PrefetchDataPipeTee& operator=(const PrefetchDataPipeTee&) = delete;

  // Returns a cloned data pipe, or a null handle when failed.
  mojo::ScopedDataPipeConsumerHandle Clone();

 private:
  friend class base::RefCounted<PrefetchDataPipeTee>;
  ~PrefetchDataPipeTee();

  // Represents a cloned output data pipe:
  // - `mojo::DataPipeProducer` holds the data pipe, and
  // - `scoped_refptr<PrefetchDataPipeTee>` keeps `PrefetchDataPipeTee` alive as
  // long as the output data pipe is still active, and should point to `this`
  // unless the `mojo::DataPipeProducer` is null.
  using ProducerPair = std::pair<std::unique_ptr<mojo::DataPipeProducer>,
                                 scoped_refptr<PrefetchDataPipeTee>>;

  void StartSourceWatcher();
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);

  // Set a new target, and returns the old target.
  ProducerPair ResetTarget(ProducerPair target);

  void OnWriteDataPipeClosed(MojoResult result,
                             const mojo::HandleSignalsState& state);

  // Writes data to `target`. This blocks reading data from `source_` until its
  // completion (i.e. `OnDataWritten()` is called).
  void WriteData(ProducerPair target, base::span<const char> data);
  void OnDataWritten(ProducerPair target, MojoResult result);

  enum class State {
    // Reading data from `source_`, and adding the data to `buffer_`.
    // `buffer_` represents the whole data read from `source_` so far.
    // The number of active cloned data pipes (not counting already closed ones)
    // is limited to up to 1 (which is `target_`), and if `target_` is not null,
    // the data is also written to `target_`.
    kLoading,

    // The data size read from `source_` exceeded the limit, and there is no
    // target.
    // `buffer_` represents the whole data read from `source_` so far.
    //
    // Reading data from `source_` is blocked, and when a new target is added by
    // `Clone()`, `buffer_` is written to the new target and cleared, and
    // transition to `kSizeExceeded`.
    kSizeExceededNoTarget,

    // The data size read from `source_` exceeded the limit.
    // Reading data from `source_` might or might not be completed.
    // `buffer_` just stores the current chunk read from `source_` while writing
    // to a target, and the early parts of the data from `source_` are already
    // discarded.
    //
    // Data read from `source_` is written to a target (`target_`, if any) in a
    // streaming fashion (i.e. the current chunk is tentatively stored in
    // `buffer_` but not accumulated).
    // If there is no target anymore, then the data can be just discarded (we
    // can't transition to `kSizeExceededNoTarget` because th data from
    // `source_` is already discarded).
    kSizeExceeded,

    // Reading data from `source_` is completed and the data is fully stored in
    // `buffer_` without reaching the buffer limit.
    // `target_` is null.
    // Any number of cloned data pipes can be created.
    kLoaded,
  };

  State state_ = State::kLoading;

  // Number of cloned data pipes waiting for `OnDataWritten()`.
  uint32_t pending_writes_ = 0;

  // The data pipe to be read, and its watcher.
  mojo::ScopedDataPipeConsumerHandle source_;
  mojo::SimpleWatcher source_watcher_;

  std::string buffer_;

  // `buffer_.size()` is limited up to `buffer_limit_`.
  const size_t buffer_limit_;

  // The cloned data pipe while `state_` is `kLoading` or `kSizeExceeded`.
  // In these states, the number of cloned data pipes is at most 1.
  // Should be modified only by `ResetTarget()`.
  ProducerPair target_;
  mojo::SimpleWatcher target_watcher_;

  base::WeakPtrFactory<PrefetchDataPipeTee> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_DATA_PIPE_TEE_H_
