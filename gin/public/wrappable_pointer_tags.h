// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_
#define GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_

#include <cstdint>

#include "v8-sandbox.h"

namespace gin {

// References from V8 JavaScript objects to C++ objects are stored with a type
// tag, and dereferencing a C++ object is only possible when the same type tag
// is used. E.g. a reference to an ArrayBuffer object can only be dereferenced
// using the ArrayBuffer type tag. This enum defines type tags for subclasses of
// `gin::Wrappable`, so that the JavaScript wrapper objects of these subclasses
// can only be unwrapped with the correct type tag.
enum WrappablePointerTag : uint16_t {
  // The type tags for gin::Wrappable start at the end of the value range to
  // avoid overlaps with the type tags of blink::ScriptWrappable.
  kFirstTag = 0x7F80,
  kTestObject = 0x7F80,  // gin::MyObject
  kTestObject2,          // gin::MyObject2
  kLastTag = kTestObject2
};

static_assert(kLastTag <
                  static_cast<uint16_t>(v8::CppHeapPointerTag::kZappedEntryTag),
              "The defined type tags exceed the range of allowed tags. Adjust "
              "the start value of this enum such that all values fit.");

}  // namespace gin

#endif  // GIN_PUBLIC_WRAPPABLE_POINTER_TAGS_H_
