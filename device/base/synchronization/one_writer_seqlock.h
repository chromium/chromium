// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_SYNCHRONIZATION_ONE_WRITER_SEQLOCK_H_
#define DEVICE_BASE_SYNCHRONIZATION_ONE_WRITER_SEQLOCK_H_

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/threading/platform_thread.h"

namespace device {

// This SeqLock handles only *one* writer and multiple readers. It may be
// suitable for low-contention with relatively infrequent writes, and many
// readers. See:
//   http://en.wikipedia.org/wiki/Seqlock
//   http://www.concurrencykit.org/doc/ck_sequence.html
// This implementation is based on ck_sequence.h from http://concurrencykit.org.
//
// Currently this type of lock is used in at least two implementations (gamepad
// and device motion, in particular see e.g. shared_memory_seqlock_buffer.h).
// It may make sense to generalize this lock to multiple writers.
//
// You must be very careful not to operate on potentially inconsistent read
// buffers. If the read must be retry'd, the data in the read buffer could
// contain any random garbage. e.g., contained pointers might be
// garbage, or indices could be out of range. Probably the only suitable thing
// to do during the read loop is to make a copy of the data, and operate on it
// only after the read was found to be consistent.
//
// Accesses to data protected by this SeqLock is recommended to be atomic.
// See:
//   https://www.hpl.hp.com/techreports/2012/HPL-2012-68.pdf
//   http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1478r1.html
class OneWriterSeqLock {
 public:
  OneWriterSeqLock();
  // ReadBegin returns |sequence_| when it is even, or when it has retried
  // |max_retries| times. Omitting |max_retries| results in ReadBegin not
  // returning until |sequence_| is even.
  base::subtle::Atomic32 ReadBegin(uint32_t max_retries = UINT32_MAX) const;
  bool ReadRetry(base::subtle::Atomic32 version) const;
  void WriteBegin();
  void WriteEnd();

 private:
  base::subtle::Atomic32 sequence_;
  DISALLOW_COPY_AND_ASSIGN(OneWriterSeqLock);
};

}  // namespace device

#endif  // DEVICE_BASE_SYNCHRONIZATION_ONE_WRITER_SEQLOCK_H_
