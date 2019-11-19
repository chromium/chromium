// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/page_orientation.h"

#include <type_traits>

namespace chrome_pdf {

namespace {

// Adds two PageOrientation values together. This works because the underlying
// integer values have been chosen to allow modular arithmetic.
PageOrientation AddOrientations(PageOrientation first, PageOrientation second) {
  using IntType = std::underlying_type<PageOrientation>::type;

  constexpr auto kOrientationCount =
      static_cast<IntType>(PageOrientation::kLast) + 1;

  auto first_int = static_cast<IntType>(first);
  auto second_int = static_cast<IntType>(second);
  return static_cast<PageOrientation>((first_int + second_int) %
                                      kOrientationCount);
}

}  // namespace

PageOrientation RotateClockwise(PageOrientation orientation) {
  return AddOrientations(orientation, PageOrientation::kClockwise90);
}

PageOrientation RotateCounterclockwise(PageOrientation orientation) {
  // Adding |kLast| is equivalent to rotating one step counterclockwise.
  return AddOrientations(orientation, PageOrientation::kLast);
}

}  // namespace chrome_pdf
