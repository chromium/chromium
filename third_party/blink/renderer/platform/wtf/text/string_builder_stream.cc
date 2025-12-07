// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"

namespace blink {

void WriteIndent(StringBuilder& builder, wtf_size_t indent) {
  for (wtf_size_t i = 0; i < indent; ++i) {
    builder.Append("  ");
  }
}

}  // namespace blink
