// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_EXCEPTION_FILTER_HELPER_WIN_H_
#define COMPONENTS_ZUCCHINI_EXCEPTION_FILTER_HELPER_WIN_H_

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/containers/span.h"

namespace zucchini {

// A helper for handling EXCEPTION_IN_PAGE_ERROR structured exceptions in
// multiple memory ranges; for example, when several files are mapped into the
// process's address space. Consumers must construct an instance outside of
// a `try-except` statement, call `AddRange` one or more times (possibly within
// the `__try` block), and call `FilterPageError` within the filter expression.
// For example:
// ```
//   ExceptionFilterHelper exception_filter_helper;
//   __try {
//     base::MemoryMappedFile mapped_file;
//     if (mapped_file.Initialize("/foo/bar")) {
//       exception_filter_helper.AddRange(mapped_file.data(),
//                                        mapped_file.length());
//       ProcessFile(mapped_file);
//     }
//   } __except(exception_filter_helper.FilterPageError(
//       GetExceptionInformation()->ExceptionRecord) {
//     // I/O error accessing the mapped file.
//   }
// ```
class ExceptionFilterHelper {
 public:
  ExceptionFilterHelper();
  ExceptionFilterHelper(const ExceptionFilterHelper&) = delete;
  ExceptionFilterHelper& operator=(const ExceptionFilterHelper&) = delete;
  ~ExceptionFilterHelper();

  // Adds a memory range within which page errors are to be handled. `range`
  // must not overlap with any previously-added range.
  void AddRange(base::span<const uint8_t> range);

  // Returns `EXCEPTION_EXECUTE_HANDLER` if `exception_record` corresponds to an
  // `EXCEPTION_IN_PAGE_ERROR` for an address wthin a range of memory previously
  // added via `AddRange`; otherwise, returns `EXCEPTION_CONTINUE_SEARCH`.
  int FilterPageError(const EXCEPTION_RECORD* const exception_record);

  // Returns the NTSTATUS of the most-recently handled exception for which
  // `FilterPageError` returned `EXCEPTION_EXECUTE_HANDLER`.
  int32_t nt_status() const { return nt_status_; }

  // Returns `true` if the most-recently handled exception for which
  // `FilterPageError` returned `EXCEPTION_EXECUTE_HANDLER` was caused by a
  // write to a mapped region; otherwise, the exception was caused by a read.
  bool is_write() const { return is_write_; }

 private:
  // Returns `true` if `address` is within any range added to the instance via
  // `AddRange`.
  bool IsInRange(const uint8_t* address) const;

  // A mapping of start address to one-past-end-address for all ranges added to
  // the instance.
  std::map<uintptr_t, uintptr_t> ranges_;

  // The NTSTATUS code of the most-recently handled exception.
  int32_t nt_status_ = 0;

  // True if the most-recently handled exception was caused by a write.
  bool is_write_ = false;
};

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_EXCEPTION_FILTER_HELPER_WIN_H_
