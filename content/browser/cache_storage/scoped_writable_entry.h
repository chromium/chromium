// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_SCOPED_WRITABLE_ENTRY_H_
#define CONTENT_BROWSER_CACHE_STORAGE_SCOPED_WRITABLE_ENTRY_H_

#include <memory>

#include "net/disk_cache/disk_cache.h"

namespace content {

// A custom deleter that closes the entry. But if WritingCompleted() hasn't been
// called, it will doom the entry before closing it.
class ScopedWritableDeleter {
 public:
  ScopedWritableDeleter() = default;
  ScopedWritableDeleter(ScopedWritableDeleter&& other) = default;
  ScopedWritableDeleter& operator=(ScopedWritableDeleter&& other) = default;

  void operator()(disk_cache::Entry* entry) {
    if (!completed_)
      entry->Doom();

    // |entry| is owned by the backend, we just need to close it as it's
    // ref-counted.
    entry->Close();
  }

  void WritingCompleted() { completed_ = true; }

 private:
  bool completed_ = false;
};

// Use this to manage disk_cache::Entry*'s that should be doomed before closing
// unless told otherwise (via calling WritingCompleted on the deleter).
//
// Example:
// ScopedWritableEntry entry(my_entry);
// .. write some stuff ..
// entry.get_deleter().WritingCompleted();
typedef std::unique_ptr<disk_cache::Entry, ScopedWritableDeleter>
    ScopedWritableEntry;

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_SCOPED_WRITABLE_ENTRY_H_
