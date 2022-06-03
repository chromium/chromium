// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_NUMBER_PARSING_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_NUMBER_PARSING_OPTIONS_H_

#include "base/check_op.h"

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace WTF {

// Copyable and immutable object representing number parsing flags.
class NumberParsingOptions {
  STACK_ALLOCATED();

 public:
  static constexpr unsigned kNone = 0;
  static constexpr unsigned kAcceptTrailingGarbage = 1;
  static constexpr unsigned kAcceptLeadingPlus = 1 << 1;
  static constexpr unsigned kAcceptLeadingTrailingWhitespace = 1 << 2;
  static constexpr unsigned kAcceptMinusZeroForUnsigned = 1 << 3;

  // 'Strict' behavior for WTF::String.
  static constexpr unsigned kStrict =
      kAcceptLeadingPlus | kAcceptLeadingTrailingWhitespace;
  // Non-'Strict' behavior for WTF::String.
  static constexpr unsigned kLoose = kStrict | kAcceptTrailingGarbage;

  // This constructor allows implicit conversion from unsigned.
  NumberParsingOptions(unsigned options) : options_(options) {
    DCHECK_LT(options, 1u << 4) << "NumberParsingOptions should be built with "
                                   "a combination of "
                                   "NumberParsingOptions::kFoo constants.";
  }

  bool AcceptTrailingGarbage() const {
    return options_ & kAcceptTrailingGarbage;
  }
  bool AcceptLeadingPlus() const { return options_ & kAcceptLeadingPlus; }
  bool AcceptWhitespace() const {
    return options_ & kAcceptLeadingTrailingWhitespace;
  }
  bool AcceptMinusZeroForUnsigned() const {
    return options_ & kAcceptMinusZeroForUnsigned;
  }

 private:
  unsigned options_;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_NUMBER_PARSING_OPTIONS_H_
