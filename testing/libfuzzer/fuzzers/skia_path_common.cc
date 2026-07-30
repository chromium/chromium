// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/fuzzers/skia_path_common.h"

#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"

// This is needed because SkPath::readFromMemory does not seem to be able to
// be able to handle arbitrary input.
SkPath BuildPath(base::SpanReader<const uint8_t>& reader, int last_verb) {
  uint8_t operation;
  SkScalar a, b, c, d, e, f;
  SkPathBuilder path;
  while (read(reader, &operation)) {
    switch (operation % (last_verb + 1)) {
      case SkPath::Verb::kMove_Verb:
        if (!read(reader, &a, &b)) {
          return path.detach();
        }
        path.moveTo(a, b);
        break;

      case SkPath::Verb::kLine_Verb:
        if (!read(reader, &a, &b)) {
          return path.detach();
        }
        path.lineTo(a, b);
        break;

      case SkPath::Verb::kQuad_Verb:
        if (!read(reader, &a, &b, &c, &d)) {
          return path.detach();
        }
        path.quadTo(a, b, c, d);
        break;

      case SkPath::Verb::kConic_Verb:
        if (!read(reader, &a, &b, &c, &d, &e)) {
          return path.detach();
        }
        path.conicTo(a, b, c, d, e);
        break;

      case SkPath::Verb::kCubic_Verb:
        if (!read(reader, &a, &b, &c, &d, &e, &f)) {
          return path.detach();
        }
        path.cubicTo(a, b, c, d, e, f);
        break;

      case SkPath::Verb::kClose_Verb:
        path.close();
        break;

      case SkPath::Verb::kDone_Verb:
        // In this case, simply exit.
        return path.detach();
    }
  }
  return path.detach();
}
