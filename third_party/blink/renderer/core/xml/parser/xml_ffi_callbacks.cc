// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xml_ffi_callbacks.h"

#include "base/strings/string_view_rust.h"

namespace xml_ffi {
namespace {

blink::AtomicString RustStrToAtomicString(rust::Str str) {
  return blink::AtomicString::FromUTF8(base::RustStrToStringView(str));
}
}  // namespace

void xml_ffi::AttributeView::Populate(rust::Str q_name_a,
                                      rust::Str attr_ns_a,
                                      rust::Str value_a) {
  this->q_name = RustStrToAtomicString(q_name_a);
  this->attr_ns = RustStrToAtomicString(attr_ns_a);
  this->value = RustStrToAtomicString(value_a);
}

}  // namespace xml_ffi
