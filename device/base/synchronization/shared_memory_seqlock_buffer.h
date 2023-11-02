// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_SYNCHRONIZATION_SHARED_MEMORY_SEQLOCK_BUFFER_H_
#define DEVICE_BASE_SYNCHRONIZATION_SHARED_MEMORY_SEQLOCK_BUFFER_H_

#include "device/base/synchronization/one_writer_seqlock.h"

namespace device {

// This structure is stored in shared memory that's shared between the browser
// which does the hardware polling, and the consumers of the data,
// i.e. the renderers. The performance characteristics are that
// we want low latency (so would like to avoid explicit communication via IPC
// between producer and consumer) and relatively large data size.
//
// Writer and reader operate on the same buffer assuming contention is low, and
// contention is detected by using the associated SeqLock.

template <class Data>
class SharedMemorySeqLockBuffer {
 public:
  OneWriterSeqLock seqlock;
  Data data;
};

}  // namespace device

#endif  // DEVICE_BASE_SYNCHRONIZATION_SHARED_MEMORY_SEQLOCK_BUFFER_H_
