// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_CHROMIUM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_CHROMIUM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/rect_chromium.h"

namespace mojo {

template <>
struct StructTraits<test::TypemappedRectDataView, test::RectChromium> {
  static int x(const test::RectChromium& r) { return r.x(); }
  static int y(const test::RectChromium& r) { return r.y(); }
  static int width(const test::RectChromium& r) { return r.width(); }
  static int height(const test::RectChromium& r) { return r.height(); }

  static bool Read(test::TypemappedRectDataView r, test::RectChromium* out) {
    if (r.width() < 0 || r.height() < 0)
      return false;
    out->set_x(r.x());
    out->set_y(r.y());
    out->set_width(r.width());
    out->set_height(r.height());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_RECT_CHROMIUM_TRAITS_H_
