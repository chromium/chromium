// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/zucchini/exception_filter_helper_win.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"

namespace zucchini {

ExceptionFilterHelper::ExceptionFilterHelper() = default;
ExceptionFilterHelper::~ExceptionFilterHelper() = default;

void ExceptionFilterHelper::AddRange(base::span<const uint8_t> range) {
  const uintptr_t range_start = reinterpret_cast<uintptr_t>(range.data());
  const uintptr_t range_end =
      base::CheckAdd(range_start, range.size()).ValueOrDie();
  auto it = ranges_.lower_bound(range_start);
  if (it != ranges_.end()) {
    CHECK_LE(range_end, it->first);
  }
  if (it != ranges_.begin()) {
    auto previous = it;
    --previous;
    CHECK_LE(previous->second, range_start);
  }
  ranges_.insert(it, {range_start, range_end});
}

int ExceptionFilterHelper::FilterPageError(
    const EXCEPTION_RECORD* const exception_record) {
  nt_status_ = 0;
  is_write_ = false;

  // For details on the contents of page error exception records, see
  // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-exception_record.
  if (exception_record->ExceptionCode != EXCEPTION_IN_PAGE_ERROR) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (exception_record->NumberParameters < 3) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  if (!IsInRange(reinterpret_cast<const uint8_t*>(
          exception_record->ExceptionInformation[1]))) {
    return EXCEPTION_CONTINUE_SEARCH;
  }

  nt_status_ = exception_record->ExceptionInformation[2];
  is_write_ = exception_record->ExceptionInformation[0] != 0;

  return EXCEPTION_EXECUTE_HANDLER;
}

bool ExceptionFilterHelper::IsInRange(const uint8_t* address) const {
  const auto addr = reinterpret_cast<uintptr_t>(address);
  auto it = ranges_.upper_bound(addr);
  if (it == ranges_.begin()) {
    return false;
  }
  --it;
  return it->first <= addr && addr < it->second;
}

}  // namespace zucchini
