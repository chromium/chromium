// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Macros specific to the RLZ library.

#ifndef RLZ_LIB_ASSERT_H_
#define RLZ_LIB_ASSERT_H_

#include <string>
#include "base/logging.h"

// An assertion macro.
// Can mute expected assertions in debug mode.

#ifndef ASSERT_STRING
  #ifndef MUTE_EXPECTED_ASSERTS
    #define ASSERT_STRING(expr) LOG_IF(FATAL, false) << (expr)
  #else
    #define ASSERT_STRING(expr) \
      do { \
        std::string expr_string(expr); \
        if (rlz_lib::expected_assertion_ != expr_string) { \
          LOG_IF(FATAL, false) << (expr); \
        } \
      } while (0)
  #endif
#endif


#ifndef VERIFY
  #ifdef _DEBUG
    #define VERIFY(expr) LOG_IF(FATAL, !(expr)) << #expr
  #else
    #define VERIFY(expr) (void)(expr)
  #endif
#endif

namespace rlz_lib {

#ifdef MUTE_EXPECTED_ASSERTS
extern std::string expected_assertion_;
#endif

inline void SetExpectedAssertion(const char* s) {
#ifdef MUTE_EXPECTED_ASSERTS
  expected_assertion_ = s;
#endif
}

}  // rlz_lib

#endif  // RLZ_LIB_ASSERT_H_
