// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_write_buffer_memory_monitor.h"

#include <utility>

#include "base/check_op.h"

namespace disk_cache {

SqlWriteBufferMemoryMonitor::ScopedReservation::ScopedReservation() = default;

SqlWriteBufferMemoryMonitor::ScopedReservation::~ScopedReservation() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (monitor_) {
    monitor_->Release(size_);
  }
}

SqlWriteBufferMemoryMonitor::ScopedReservation::ScopedReservation(
    ScopedReservation&& other)
    : monitor_(std::exchange(other.monitor_, nullptr)),
      size_(std::exchange(other.size_, 0)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

SqlWriteBufferMemoryMonitor::ScopedReservation&
SqlWriteBufferMemoryMonitor::ScopedReservation::operator=(
    ScopedReservation&& other) {
  if (this != &other) {
    monitor_ = std::exchange(other.monitor_, nullptr);
    size_ = std::exchange(other.size_, 0);
  }
  return *this;
}

void SqlWriteBufferMemoryMonitor::ScopedReservation::IncreaseSize(
    base::PassKey<SqlWriteBufferMemoryMonitor>,
    base::WeakPtr<SqlWriteBufferMemoryMonitor> monitor,
    int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!monitor_ || monitor_.get() == monitor.get());
  size_ += size;
  if (!monitor_) {
    monitor_ = std::move(monitor);
  }
}

SqlWriteBufferMemoryMonitor::SqlWriteBufferMemoryMonitor(int64_t max_size)
    : max_size_(max_size) {}

SqlWriteBufferMemoryMonitor::~SqlWriteBufferMemoryMonitor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

int64_t SqlWriteBufferMemoryMonitor::CurrentSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return current_size_;
}

bool SqlWriteBufferMemoryMonitor::Allocate(int size,
                                           ScopedReservation& reservation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(size, 0);

  if (current_size_ + size > max_size_) {
    return false;
  }

  if (size == 0) {
    return true;
  }

  current_size_ += size;
  reservation.IncreaseSize(base::PassKey<SqlWriteBufferMemoryMonitor>(),
                           weak_factory_.GetWeakPtr(), size);
  return true;
}

void SqlWriteBufferMemoryMonitor::Release(int64_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_size_ -= size;
  DCHECK_GE(current_size_, 0);
}

}  // namespace disk_cache
