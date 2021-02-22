// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/union_base.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace bindings {

// static
String UnionBase::ProduceUnionNameInIDL(
    const base::span<const char* const>& member_names) {
  DCHECK_GE(member_names.size(), 2u);

  StringBuilder builder;

  builder.Append("(");
  builder.Append(member_names[0]);
  for (size_t i = 1; i < member_names.size(); ++i) {
    builder.Append(" or ");
    builder.Append(member_names[i]);
  }
  builder.Append(")");

  return builder.ToString();
}

}  // namespace bindings

}  // namespace blink
