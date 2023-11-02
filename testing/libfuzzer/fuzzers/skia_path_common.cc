// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/fuzzers/skia_path_common.h"

#include "third_party/skia/include/core/SkPath.h"

// This is needed because SkPath::readFromMemory does not seem to be able to
// be able to handle arbitrary input.
void BuildPath(const uint8_t** data,
               size_t* size,
               SkPath* path,
               int last_verb) {
  uint8_t operation;
  SkScalar a, b, c, d, e, f;
  while (read<uint8_t>(data, size, &operation)) {
    switch (operation % (last_verb + 1)) {
      case SkPath::Verb::kMove_Verb:
        if (!read<SkScalar>(data, size, &a) || !read<SkScalar>(data, size, &b))
          return;
        path->moveTo(a, b);
        break;

      case SkPath::Verb::kLine_Verb:
        if (!read<SkScalar>(data, size, &a) || !read<SkScalar>(data, size, &b))
          return;
        path->lineTo(a, b);
        break;

      case SkPath::Verb::kQuad_Verb:
        if (!read<SkScalar>(data, size, &a) ||
            !read<SkScalar>(data, size, &b) ||
            !read<SkScalar>(data, size, &c) ||
            !read<SkScalar>(data, size, &d))
          return;
        path->quadTo(a, b, c, d);
        break;

      case SkPath::Verb::kConic_Verb:
        if (!read<SkScalar>(data, size, &a) ||
            !read<SkScalar>(data, size, &b) ||
            !read<SkScalar>(data, size, &c) ||
            !read<SkScalar>(data, size, &d) ||
            !read<SkScalar>(data, size, &e))
          return;
        path->conicTo(a, b, c, d, e);
        break;

      case SkPath::Verb::kCubic_Verb:
        if (!read<SkScalar>(data, size, &a) ||
            !read<SkScalar>(data, size, &b) ||
            !read<SkScalar>(data, size, &c) ||
            !read<SkScalar>(data, size, &d) ||
            !read<SkScalar>(data, size, &e) ||
            !read<SkScalar>(data, size, &f))
          return;
        path->cubicTo(a, b, c, d, e, f);
        break;

      case SkPath::Verb::kClose_Verb:
        path->close();
        break;

      case SkPath::Verb::kDone_Verb:
        // In this case, simply exit.
        return;
    }
  }
}
