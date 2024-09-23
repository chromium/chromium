// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/struct_with_traits_impl_traits.h"

namespace mojo {

// static
int32_t StructTraits<test::NestedStructWithTraitsDataView,
                     test::NestedStructWithTraitsImpl>::
    value(const test::NestedStructWithTraitsImpl& input) {
  return input.value;
}

// static
bool StructTraits<test::NestedStructWithTraitsDataView,
                  test::NestedStructWithTraitsImpl>::
    Read(test::NestedStructWithTraits::DataView data,
         test::NestedStructWithTraitsImpl* output) {
  output->value = data.value();
  return true;
}

test::EnumWithTraits
EnumTraits<test::EnumWithTraits, test::EnumWithTraitsImpl>::ToMojom(
    test::EnumWithTraitsImpl input) {
  switch (input) {
    case test::EnumWithTraitsImpl::CUSTOM_VALUE_0:
      return test::EnumWithTraits::VALUE_0;
    case test::EnumWithTraitsImpl::CUSTOM_VALUE_1:
      return test::EnumWithTraits::VALUE_1;
  };

  NOTREACHED();
}

bool EnumTraits<test::EnumWithTraits, test::EnumWithTraitsImpl>::FromMojom(
    test::EnumWithTraits input,
    test::EnumWithTraitsImpl* output) {
  switch (input) {
    case test::EnumWithTraits::VALUE_0:
      *output = test::EnumWithTraitsImpl::CUSTOM_VALUE_0;
      return true;
    case test::EnumWithTraits::VALUE_1:
      *output = test::EnumWithTraitsImpl::CUSTOM_VALUE_1;
      return true;
  };

  return false;
}

// static
bool StructTraits<test::StructWithTraitsDataView, test::StructWithTraitsImpl>::
    Read(test::StructWithTraits::DataView data,
         test::StructWithTraitsImpl* out) {
  test::EnumWithTraitsImpl f_enum;
  if (!data.ReadFEnum(&f_enum))
    return false;
  out->set_enum(f_enum);

  out->set_bool(data.f_bool());
  out->set_uint32(data.f_uint32());
  out->set_uint64(data.f_uint64());

  std::string_view f_string;
  std::string f_string2;
  if (!data.ReadFString(&f_string) || !data.ReadFString2(&f_string2) ||
      f_string != f_string2) {
    return false;
  }
  out->set_string(f_string2);

  if (!data.ReadFStringArray(&out->get_mutable_string_array()))
    return false;

  // We can't deserialize as a std::set, so we have to manually copy from the
  // data view.
  ArrayDataView<StringDataView> string_set_data_view;
  data.GetFStringSetDataView(&string_set_data_view);
  for (size_t i = 0; i < string_set_data_view.size(); ++i) {
    std::string value;
    string_set_data_view.Read(i, &value);
    out->get_mutable_string_set().insert(value);
  }

  if (!data.ReadFStruct(&out->get_mutable_struct()))
    return false;

  if (!data.ReadFStructArray(&out->get_mutable_struct_array()))
    return false;

  if (!data.ReadFStructMap(&out->get_mutable_struct_map()))
    return false;

  return true;
}

// static
bool StructTraits<test::MoveOnlyStructWithTraitsDataView,
                  test::MoveOnlyStructWithTraitsImpl>::
    Read(test::MoveOnlyStructWithTraits::DataView data,
         test::MoveOnlyStructWithTraitsImpl* out) {
  out->get_mutable_handle() = data.TakeFHandle();
  return true;
}

}  // namespace mojo
