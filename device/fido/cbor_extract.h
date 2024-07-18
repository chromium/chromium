// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef DEVICE_FIDO_CBOR_EXTRACT_H_
#define DEVICE_FIDO_CBOR_EXTRACT_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/cbor/values.h"

namespace device {
namespace cbor_extract {

// cbor_extract implements a framework for pulling select members out of a
// cbor::Value and checking that they have the expected type. It is intended for
// use in contexts where code-size is important.
//
// The top-level cbor::Value must be a map. The extraction is driven by a series
// of commands specified by an array of StepOrByte. There are helper functions
// below for constructing the StepOrByte values and using them is strongly
// advised because they're constexprs, thus have no code-size cost, but they can
// statically check for some mistakes.
//
// As an example, consider a CBOR map {1: 2}. In order to extract the member
// with key '1', we can use:
//
//   struct MyObj {
//     const int64_t *value;
//   };
//
//   static constexpr StepOrByte<MyObj> kSteps[] = {
//       ELEMENT(Is::kRequired, MyObj, value),
//       IntKey<MyObj>(1),
//       Stop<MyObj>(),
//   };
//
// Note that a Stop() is required at the end of every map. If you have nested
// maps, they are deliminated by Stop()s.
//
// ELEMENT specifies an extraction output and whether it's required to have been
// found in the input. The target structure should only contain pointers and the
// CBOR type is taken from the type of the struct member. (See comments in
// |Type| for the list of C++ types.) A value with an incorrect CBOR type is an
// error, even if marked optional. Missing optional values result in |nullptr|.
//
// Only 16 pointers in the output structure can be addressed. Referencing a
// member past the 15th is a compile-time error.
//
// Output values are also pointers into the input cbor::Value, so that cannot
// be destroyed until processing is complete.
//
// Keys for the element are either specified by IntKey<S>(x), where -128 <= x <
// 127, or StringKey<S>() followed by a NUL-terminated string:
//
//   static constexpr StepOrByte<MyObj> kSteps[] = {
//       ELEMENT(Is::kRequired, MyObj, value),
//       StringKey<MyObj>(), 'k', 'e', 'y', '\0',
//       Stop<MyObj>(),
//   };
//
// Maps are recursed into and do not result in an output value. (If you want to
// extract a map itself, have an output with type |const cbor::Value *|.)
//
//   static constexpr StepOrByte<MyObj> kSteps[] = {
//      Map<MyObj>(),
//      IntKey<MyObj>(2),
//        ELEMENT(Is::kRequired, MyObj, value),
//        StringKey<MyObj>(), 'k', 'e', 'y', '\0',
//      Stop<MyObj>(),
//   };
//
// The target structure names gets repeated a lot. That's C++ templates for you.
//
// Because the StepOrByte helper functions are constexpr, the steps can be
// evaluated at compile time to produce a compact array of bytes. Each element
// takes a single byte.

enum class Is {
  kRequired,
  kOptional,
};

namespace internal {

// Type reflects the type of the struct members that can be given to ELEMENT().
enum class Type {   // Output type
  kBytestring = 0,  // const std::vector<uint8_t>*
  kString = 1,      // const std::string*
  kBoolean = 2,     // const bool*
  kInt = 3,         // const int64_t*
  kMap = 4,         // <no output>
  kArray = 5,       // const std::vector<cbor::Value>*
  kValue = 6,       // const cbor::Value*
  kStop = 7,        // <no output>
};

// Step is an internal detail that needs to be in the header file in order to
// work.
struct Step {
  Step() = default;
  constexpr Step(uint8_t in_required,
                 uint8_t in_value_type,
                 uint8_t in_output_index)
      : required(in_required),
        value_type(in_value_type),
        output_index(in_output_index) {}

  struct {
    bool required : 1;
    uint8_t value_type : 3;
    uint8_t output_index : 4;
  };
};

}  // namespace internal

// StepOrByte is an internal detail that needs to be in the header file in order
// to work.
template <typename S>
struct StepOrByte {
  // STRING_KEY is the magic value of |u8| that indicates that this is not an
  // integer key, but the a NUL-terminated string follows.
  static constexpr uint8_t STRING_KEY = 127;

  constexpr explicit StepOrByte(const internal::Step& in_step)
      : step(in_step) {}
  constexpr explicit StepOrByte(uint8_t b) : u8(b) {}
  // This constructor is deliberately not |explicit| so that it's possible to
  // write string keys in the steps array.
  constexpr StepOrByte(char in_c) : c(in_c) {}

  union {
    char c;
    uint8_t u8;
    internal::Step step;
  };
};

#define ELEMENT(required, clas, member)                              \
  ::device::cbor_extract::internal::Element(required, &clas::member, \
                                            offsetof(clas, member))

template <typename S>
constexpr StepOrByte<S> IntKey(int key) {
  if (key > std::numeric_limits<int8_t>::max() ||
      key < std::numeric_limits<int8_t>::min() ||
      key == StepOrByte<S>::STRING_KEY) {
    // It's a compile-time error if __builtin_unreachable is reachable.
    __builtin_unreachable();
  }
  return StepOrByte<S>(static_cast<char>(key));
}

template <typename S>
constexpr StepOrByte<S> StringKey() {
  return StepOrByte<S>(static_cast<char>(StepOrByte<S>::STRING_KEY));
}

template <typename S>
constexpr StepOrByte<S> Map(const Is required = Is::kRequired) {
  return StepOrByte<S>(
      internal::Step(required == Is::kRequired,
                     static_cast<uint8_t>(internal::Type::kMap), -1));
}

template <typename S>
constexpr StepOrByte<S> Stop() {
  return StepOrByte<S>(
      internal::Step(false, static_cast<uint8_t>(internal::Type::kStop), 0));
}

namespace internal {

template <typename S, typename T>
constexpr StepOrByte<S> Element(const Is required,
                                T S::*member,
                                uintptr_t offset) {
  // This generic version of |Element| causes a compile-time error if ELEMENT
  // is used to reference a member with an invalid type.
  __builtin_unreachable();
  return StepOrByte<S>('\0');
}

// MemberNum translates an offset into a structure into an index if the
// structure is considered as an array of pointers.
constexpr uint8_t MemberNum(uintptr_t offset) {
  if (offset % sizeof(void*)) {
    __builtin_unreachable();
  }
  const uintptr_t index = offset / sizeof(void*);
  if (index >= 16) {
    __builtin_unreachable();
  }
  return static_cast<uint8_t>(index);
}

template <typename S>
constexpr StepOrByte<S> ElementImpl(const Is required,
                                    uintptr_t offset,
                                    internal::Type type) {
  return StepOrByte<S>(internal::Step(required == Is::kRequired,
                                      static_cast<uint8_t>(type),
                                      MemberNum(offset)));
}

// These are specialisations of Element for each of the value output types.

template <typename S>
constexpr StepOrByte<S> Element(const Is required,
                                const std::vector<uint8_t>* S::*member,
                                uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kBytestring);
}

template <typename S>
constexpr StepOrByte<S> Element(const Is required,
                                const std::string* S::*member,
                                uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kString);
}

template <typename S>
constexpr StepOrByte<S> Element(const Is required,
                                const int64_t* S::*member,
                                uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kInt);
}

template <typename S>
constexpr StepOrByte<S> Element(const Is required,
                                const std::vector<cbor::Value>* S::*member,
                                uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kArray);
}

template <typename S>
constexpr StepOrByte<S> Element(const Is required,
                                const cbor::Value* S::*member,
                                uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kValue);
}

template <typename S>
constexpr StepOrByte<S> Element(const Is required,
                                const bool* S::*member,
                                uintptr_t offset) {
  return ElementImpl<S>(required, offset, internal::Type::kBoolean);
}

COMPONENT_EXPORT(DEVICE_FIDO)
bool Extract(base::span<const void*> outputs,
             base::span<const StepOrByte<void>> steps,
             const cbor::Value::MapValue& map);

}  // namespace internal

template <typename S>
bool Extract(S* output,
             base::span<const StepOrByte<S>> steps,
             const cbor::Value::MapValue& map) {
  // The compiler enforces that |output| points to the correct type and this
  // code then erases those types for use in the non-templated internal code. We
  // don't want to template the internal code because we don't want the compiler
  // to generate a copy for every type.
  static_assert(sizeof(S) % sizeof(void*) == 0, "struct contains non-pointers");
  static_assert(sizeof(S) >= sizeof(void*),
                "empty output structures are invalid, even if you just want to "
                "check that maps exist, because the code unconditionally "
                "indexes offset zero.");
  base::span<const void*> outputs(reinterpret_cast<const void**>(output),
                                  sizeof(S) / sizeof(void*));
  base::span<const StepOrByte<void>> steps_void(
      reinterpret_cast<const StepOrByte<void>*>(steps.data()), steps.size());
  return internal::Extract(outputs, steps_void, map);
}

// ForEachPublicKeyEntry is a helper for dealing with CTAP2 structures. It takes
// an array and, for each value in the array, it expects the value to be a map
// and, in the map, it expects the key "type" to result in a string. If that
// string is not "public-key", it ignores the array element. Otherwise it looks
// up |key| in the map and passes it to |callback|.
COMPONENT_EXPORT(DEVICE_FIDO)
bool ForEachPublicKeyEntry(
    const cbor::Value::ArrayValue& array,
    const cbor::Value& key,
    base::RepeatingCallback<bool(const cbor::Value&)> callback);

}  // namespace cbor_extract
}  // namespace device

#endif  // DEVICE_FIDO_CBOR_EXTRACT_H_
