// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_USE_AFTER_FREE_CHECKER_H_
#define MEDIA_BASE_USE_AFTER_FREE_CHECKER_H_

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "media/base/media_export.h"

namespace {

static base::debug::CrashKeyString* crash_key_string() {
  static base::debug::CrashKeyString* string = nullptr;
  if (!string) {
    string = base::debug::AllocateCrashKeyString(
        "use_after_free_checker", base::debug::CrashKeySize::Size32);
  }

  return string;
}

}  // namespace

namespace media {

// Maintains a guard value from ctor => dtor, and causes a crash if it's ever
// found to be in an incorrect state.  Includes a crash key that identifies
// whether itse use-after-free or corruption.
class MEDIA_EXPORT UseAfterFreeChecker {
 public:
  inline ~UseAfterFreeChecker() {
    check();
    state_ = State::kDestructed;
  }

  void check() const {
    if (state_ != State::kConstructed) {
      base::debug::ScopedCrashKeyString scoped(
          crash_key_string(),
          state_ == State::kDestructed ? "destructed" : "corrupt");
      CHECK(false);
    }
  }

 private:
  enum State : uint64_t {
    kConstructed = 0x80C0FFEE,
    kDestructed = 0xDEADC0DE,
  };

  // Declare this as an int rather than State, to avoid undefinedness if it gets
  // overwritten with an arbitrary value.
  uint64_t state_ = State::kConstructed;
};

}  // namespace media

#endif  // MEDIA_BASE_USE_AFTER_FREE_CHECKER_H_
