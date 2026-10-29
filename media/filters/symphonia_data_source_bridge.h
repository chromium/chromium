// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SYMPHONIA_DATA_SOURCE_BRIDGE_H_
#define MEDIA_FILTERS_SYMPHONIA_DATA_SOURCE_BRIDGE_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "media/base/media_export.h"
#include "media/filters/symphonia_source_bridge.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace media {

class DataSource;

// An implementation of `SymphoniaSourceBridge` that adapts an asynchronous
// `DataSource` for synchronous reading and seeking by Symphonia on a sequenced
// blocking task runner.
//
// Instances can only be created via `Create()`, which returns a `UniquePtr`
// whose custom deleter stops `data_source`, unblocks any pending read, and
// destroys the bridge on `blocking_task_runner` after any in-flight task
// completes.
class MEDIA_EXPORT SymphoniaDataSourceBridge final
    : public SymphoniaSourceBridge {
 public:
  struct MEDIA_EXPORT Deleter {
    void operator()(SymphoniaDataSourceBridge* bridge) const;
  };

  using UniquePtr = std::unique_ptr<SymphoniaDataSourceBridge, Deleter>;

  // Creates a bridge that reads and seeks `data_source` on
  // `blocking_task_runner` (which must allow base sync primitives when backed
  // by `base::ThreadPool`). `error_cb` is posted back to the sequence that
  // called `Create()` whenever `DataSource::Read()` reports a read error.
  //
  // `data_source` must remain valid until the returned `UniquePtr` is reset or
  // destroyed.
  static UniquePtr Create(
      DataSource* data_source,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      base::RepeatingClosure error_cb);

  SymphoniaDataSourceBridge(const SymphoniaDataSourceBridge&) = delete;
  SymphoniaDataSourceBridge& operator=(const SymphoniaDataSourceBridge&) =
      delete;

  // SymphoniaSourceBridge implementation. Must be called on
  // `blocking_task_runner`.
  int64_t Read(rust::Slice<uint8_t> buf) override;
  bool Seek(uint64_t pos) override;
  uint64_t GetPosition() const override;
  int64_t GetLength() const override;
  bool IsSeekable() const override;

 private:
  struct ReadOperation;

  SymphoniaDataSourceBridge(
      DataSource* data_source,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      base::RepeatingClosure error_cb);

  // Only invoked via `Deleter` on `blocking_task_runner` after `Shutdown()` has
  // stopped `data_source_` and signaled `aborted_`.
  ~SymphoniaDataSourceBridge() override;

  // Stops `data_source_` completely and signals `aborted_` so any blocked
  // `Read()` returns right away and `data_source_` is not used again.
  void Shutdown();

  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  const base::RepeatingClosure error_cb_;
  const bool is_streaming_;

  mutable base::Lock lock_;
  raw_ptr<DataSource> data_source_ GUARDED_BY(lock_);

  // Signaled on `Shutdown()` or after an unrecoverable `DataSource::kReadError`
  // to unblock any pending read and reject subsequent reads and seeks.
  base::WaitableEvent aborted_;

  int64_t read_position_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Reused for every read. Only touched by `Read()` on `blocking_task_runner_`
  // and by the completion callback of the read in flight.
  const scoped_refptr<base::RefCountedData<ReadOperation>> read_op_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SYMPHONIA_DATA_SOURCE_BRIDGE_H_
