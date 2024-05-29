// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

WTF::StringView CannotReturnStringViewOfLocalString() {
  WTF::String local_string{};
  return local_string; // expected-error {{address of stack memory associated with local variable 'local_string' returned}}
}

WTF::String ReturnsAString() {
  return {};
}

void CannotConstructStringViewThatOutlivesTemporaryString() {
  WTF::StringView sv{ReturnsAString()}; // expected-error {{temporary whose address is used as value of local variable 'sv' will be destroyed at the end of the full-expression}}

  // Also check the constructors that take offsets.
  WTF::StringView sv2{ReturnsAString(), 0}; // expected-error {{temporary whose address is used as value of local variable 'sv2' will be destroyed at the end of the full-expression}}
  WTF::StringView sv3{ReturnsAString(), 0, 0}; // expected-error {{temporary whose address is used as value of local variable 'sv3' will be destroyed at the end of the full-expression}}
}

WTF::StringView CannotReturnStringViewOfTemporaryString() {
  return ReturnsAString(); // expected-error {{returning address of local temporary object}}
}

WTF::StringView CannotReturnStringViewOfLocalAtomicString() {
  WTF::AtomicString local_string{};
  return local_string; // expected-error {{address of stack memory associated with local variable 'local_string' returned}}
}

WTF::AtomicString ReturnsAnAtomicString() {
  return {};
}

void CannotConstructStringViewThatOutlivesTemporaryAtomicString() {
  WTF::StringView sv{ReturnsAnAtomicString()}; // expected-error {{temporary whose address is used as value of local variable 'sv' will be destroyed at the end of the full-expression}}

  // Also check the constructors that take offsets.
  WTF::StringView sv2{ReturnsAnAtomicString(), 0}; // expected-error {{temporary whose address is used as value of local variable 'sv2' will be destroyed at the end of the full-expression}}
  WTF::StringView sv3{ReturnsAnAtomicString(), 0, 0}; // expected-error {{temporary whose address is used as value of local variable 'sv3' will be destroyed at the end of the full-expression}}
}

WTF::StringView CannotReturnStringViewOfTemporaryAtomicString() {
  return ReturnsAnAtomicString(); // expected-error {{returning address of local temporary object}}
}
