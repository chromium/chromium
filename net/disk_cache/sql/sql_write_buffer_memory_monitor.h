// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_WRITE_BUFFER_MEMORY_MONITOR_H_
#define NET_DISK_CACHE_SQL_SQL_WRITE_BUFFER_MEMORY_MONITOR_H_

#include <cstdint>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "net/base/net_export.h"

namespace disk_cache {

// A helper class to monitor and limit the total memory usage of write buffers.
// This class is not thread-safe and must be accessed on the same sequence.
class NET_EXPORT_PRIVATE SqlWriteBufferMemoryMonitor {
 public:
  // A helper class that holds a reservation for a specific amount of memory
  // from the monitor. When this object is destroyed, it automatically releases
  // the reserved memory back to the monitor.
  // This class is move-only to ensure clear ownership of the reservation.
  class NET_EXPORT_PRIVATE ScopedReservation {
   public:
    ScopedReservation();
    ~ScopedReservation();

    ScopedReservation(const ScopedReservation&) = delete;
    ScopedReservation& operator=(const ScopedReservation&) = delete;

    ScopedReservation(ScopedReservation&& other);
    ScopedReservation& operator=(ScopedReservation&& other);

    // Increases the size tracked by this reservation and the monitor.
    void IncreaseSize(base::PassKey<SqlWriteBufferMemoryMonitor>,
                      base::WeakPtr<SqlWriteBufferMemoryMonitor> monitor,
                      int64_t size);

   private:
    base::WeakPtr<SqlWriteBufferMemoryMonitor> monitor_;
    int64_t size_ = 0;
    SEQUENCE_CHECKER(sequence_checker_);
  };

  explicit SqlWriteBufferMemoryMonitor(int64_t max_size);
  ~SqlWriteBufferMemoryMonitor();

  SqlWriteBufferMemoryMonitor(const SqlWriteBufferMemoryMonitor&) = delete;
  SqlWriteBufferMemoryMonitor& operator=(const SqlWriteBufferMemoryMonitor&) =
      delete;

  // Returns the current total size of allocated memory.
  int64_t CurrentSize();

  // Tries to allocate `size` bytes. Returns true if the allocation is
  // successful (i.e., the total size does not exceed `max_size_`). If
  // successful, `reservation` is updated to track the allocated memory.
  bool Allocate(int size, ScopedReservation& reservation);

 private:
  friend class ScopedReservation;
  void Release(int64_t size);

  const int64_t max_size_;

  int64_t current_size_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SqlWriteBufferMemoryMonitor> weak_factory_{this};
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_WRITE_BUFFER_MEMORY_MONITOR_H_
