// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_SHARED_RECT_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_SHARED_RECT_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/shared_rect.h"
#include "mojo/public/interfaces/bindings/tests/rect.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<test::SharedTypemappedRectDataView, test::SharedRect> {
  static int x(const test::SharedRect& r) { return r.x(); }
  static int y(const test::SharedRect& r) { return r.y(); }
  static int width(const test::SharedRect& r) { return r.width(); }
  static int height(const test::SharedRect& r) { return r.height(); }

  static bool Read(test::SharedTypemappedRectDataView r,
                   test::SharedRect* out) {
    out->set_x(r.x());
    out->set_y(r.y());
    out->set_width(r.width());
    out->set_height(r.height());
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_SHARED_RECT_TRAITS_H_
