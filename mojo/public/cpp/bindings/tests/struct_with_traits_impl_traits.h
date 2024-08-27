// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl.h"
#include "mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.h"

namespace mojo {

template <>
struct StructTraits<test::NestedStructWithTraitsDataView,
                    test::NestedStructWithTraitsImpl> {
  static int32_t value(const test::NestedStructWithTraitsImpl& input);

  static bool Read(test::NestedStructWithTraitsDataView data,
                   test::NestedStructWithTraitsImpl* output);
};

template <>
struct EnumTraits<test::EnumWithTraits, test::EnumWithTraitsImpl> {
  static test::EnumWithTraits ToMojom(test::EnumWithTraitsImpl input);
  static bool FromMojom(test::EnumWithTraits input,
                        test::EnumWithTraitsImpl* output);
};

template <>
struct StructTraits<test::StructWithTraitsDataView,
                    test::StructWithTraitsImpl> {
  // Deserialization to test::StructTraitsImpl.
  static bool Read(test::StructWithTraitsDataView data,
                   test::StructWithTraitsImpl* out);

  // Fields in test::StructWithTraits.
  // See src/mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.
  static test::EnumWithTraitsImpl f_enum(
      const test::StructWithTraitsImpl& value) {
    return value.get_enum();
  }

  static bool f_bool(const test::StructWithTraitsImpl& value) {
    return value.get_bool();
  }

  static uint32_t f_uint32(const test::StructWithTraitsImpl& value) {
    return value.get_uint32();
  }

  static uint64_t f_uint64(const test::StructWithTraitsImpl& value) {
    return value.get_uint64();
  }

  static std::string_view f_string(const test::StructWithTraitsImpl& value) {
    return value.get_string_as_string_piece();
  }

  static const std::string& f_string2(const test::StructWithTraitsImpl& value) {
    return value.get_string();
  }

  static const std::vector<std::string>& f_string_array(
      const test::StructWithTraitsImpl& value) {
    return value.get_string_array();
  }

  static const std::set<std::string>& f_string_set(
      const test::StructWithTraitsImpl& value) {
    return value.get_string_set();
  }

  static const test::NestedStructWithTraitsImpl& f_struct(
      const test::StructWithTraitsImpl& value) {
    return value.get_struct();
  }

  static const std::vector<test::NestedStructWithTraitsImpl>& f_struct_array(
      const test::StructWithTraitsImpl& value) {
    return value.get_struct_array();
  }

  static const std::map<std::string, test::NestedStructWithTraitsImpl>&
  f_struct_map(const test::StructWithTraitsImpl& value) {
    return value.get_struct_map();
  }
};

template <>
struct StructTraits<test::StructWithUnreachableTraitsDataView,
                    test::StructWithUnreachableTraitsImpl> {
 public:
  static bool ignore_me(const test::StructWithUnreachableTraitsImpl& input) {
    NOTREACHED();
  }

  static bool Read(test::StructWithUnreachableTraitsDataView data,
                   test::StructWithUnreachableTraitsImpl* out) {
    NOTREACHED();
  }
};

template <>
struct StructTraits<test::TrivialStructWithTraitsDataView,
                    test::TrivialStructWithTraitsImpl> {
  // Deserialization to test::TrivialStructTraitsImpl.
  static bool Read(test::TrivialStructWithTraitsDataView data,
                   test::TrivialStructWithTraitsImpl* out) {
    out->value = data.value();
    return true;
  }

  // Fields in test::TrivialStructWithTraits.
  // See src/mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.
  static int32_t value(test::TrivialStructWithTraitsImpl& input) {
    return input.value;
  }
};

template <>
struct StructTraits<test::MoveOnlyStructWithTraitsDataView,
                    test::MoveOnlyStructWithTraitsImpl> {
  // Deserialization to test::MoveOnlyStructTraitsImpl.
  static bool Read(test::MoveOnlyStructWithTraitsDataView data,
                   test::MoveOnlyStructWithTraitsImpl* out);

  // Fields in test::MoveOnlyStructWithTraits.
  // See src/mojo/public/interfaces/bindings/tests/struct_with_traits.mojom.
  static ScopedHandle f_handle(test::MoveOnlyStructWithTraitsImpl& value) {
    return std::move(value.get_mutable_handle());
  }
};

template <>
struct StructTraits<test::StructWithTraitsForUniquePtrDataView,
                    std::unique_ptr<int>> {
  static bool IsNull(const std::unique_ptr<int>& data) { return !data; }
  static void SetToNull(std::unique_ptr<int>* data) { data->reset(); }

  static int f_int32(const std::unique_ptr<int>& data) { return *data; }

  static bool Read(test::StructWithTraitsForUniquePtrDataView data,
                   std::unique_ptr<int>* out) {
    out->reset(new int(data.f_int32()));
    return true;
  }
};

template <>
struct UnionTraits<test::UnionWithTraitsDataView,
                   std::unique_ptr<test::UnionWithTraitsBase>> {
  static bool IsNull(const std::unique_ptr<test::UnionWithTraitsBase>& data) {
    return !data;
  }
  static void SetToNull(std::unique_ptr<test::UnionWithTraitsBase>* data) {
    data->reset();
  }

  static test::UnionWithTraitsDataView::Tag GetTag(
      const std::unique_ptr<test::UnionWithTraitsBase>& data) {
    if (data->type() == test::UnionWithTraitsBase::Type::INT32)
      return test::UnionWithTraitsDataView::Tag::kFInt32;

    return test::UnionWithTraitsDataView::Tag::kFStruct;
  }

  static int32_t f_int32(
      const std::unique_ptr<test::UnionWithTraitsBase>& data) {
    return static_cast<test::UnionWithTraitsInt32*>(data.get())->value();
  }

  static const test::NestedStructWithTraitsImpl& f_struct(
      const std::unique_ptr<test::UnionWithTraitsBase>& data) {
    return static_cast<test::UnionWithTraitsStruct*>(data.get())->get_struct();
  }

  static bool Read(test::UnionWithTraitsDataView data,
                   std::unique_ptr<test::UnionWithTraitsBase>* out) {
    switch (data.tag()) {
      case test::UnionWithTraitsDataView::Tag::kFInt32: {
        out->reset(new test::UnionWithTraitsInt32(data.f_int32()));
        return true;
      }
      case test::UnionWithTraitsDataView::Tag::kFStruct: {
        auto* struct_object = new test::UnionWithTraitsStruct();
        out->reset(struct_object);
        return data.ReadFStruct(&struct_object->get_mutable_struct());
      }
    }

    NOTREACHED();
  }
};

template <>
struct StructTraits<test::StructForceSerializeDataView,
                    test::StructForceSerializeImpl> {
  static int32_t value(const test::StructForceSerializeImpl& impl) {
    impl.set_was_serialized();
    return impl.value();
  }

  static bool Read(test::StructForceSerializeDataView data,
                   test::StructForceSerializeImpl* out) {
    out->set_value(data.value());
    out->set_was_deserialized();
    return true;
  }
};

template <>
struct StructTraits<test::StructNestedForceSerializeDataView,
                    test::StructNestedForceSerializeImpl> {
  static const test::StructForceSerializeImpl& force(
      const test::StructNestedForceSerializeImpl& impl) {
    impl.set_was_serialized();
    return impl.force();
  }

  static bool Read(test::StructNestedForceSerializeDataView data,
                   test::StructNestedForceSerializeImpl* out) {
    if (!data.ReadForce(&out->force()))
      return false;
    out->set_was_deserialized();
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_TRAITS_H_
