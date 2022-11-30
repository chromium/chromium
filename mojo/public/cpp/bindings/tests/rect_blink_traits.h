// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_BLINK_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_BLINK_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/rect_blink.h"
#include "mojo/public/interfaces/bindings/tests/rect.mojom-blink.h"

namespace mojo {

template <>
struct StructTraits<test::TypemappedRectDataView, test::RectBlink> {
  static int x(const test::RectBlink& r) { return r.x(); }
  static int y(const test::RectBlink& r) { return r.y(); }
  static int width(const test::RectBlink& r) { return r.width(); }
  static int height(const test::RectBlink& r) { return r.height(); }

  static bool Read(test::TypemappedRectDataView r, test::RectBlink* out) {
    if (r.x() < 0 || r.y() < 0 || r.width() < 0 || r.height() < 0) {
      return false;
    }
    out->setX(r.x());
    out->setY(r.y());
    out->setWidth(r.width());
    out->setHeight(r.height());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_BLINK_TRAITS_H_
