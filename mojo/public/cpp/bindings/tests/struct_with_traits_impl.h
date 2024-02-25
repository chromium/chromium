// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "mojo/public/cpp/system/handle.h"

namespace mojo {
namespace test {

struct NestedStructWithTraitsImpl {
 public:
  NestedStructWithTraitsImpl();
  explicit NestedStructWithTraitsImpl(int32_t in_value);

  bool operator==(const NestedStructWithTraitsImpl& other) const {
    return value == other.value;
  }

  int32_t value = 0;
};

enum class EnumWithTraitsImpl { CUSTOM_VALUE_0 = 10, CUSTOM_VALUE_1 = 11 };

// A type which knows how to look like a mojo::test::StructWithTraits mojom type
// by way of mojo::StructTraits.
class StructWithTraitsImpl {
 public:
  StructWithTraitsImpl();
  ~StructWithTraitsImpl();

  StructWithTraitsImpl(const StructWithTraitsImpl& other);

  void set_enum(EnumWithTraitsImpl value) { enum_ = value; }
  EnumWithTraitsImpl get_enum() const { return enum_; }

  void set_bool(bool value) { bool_ = value; }
  bool get_bool() const { return bool_; }

  void set_uint32(uint32_t value) { uint32_ = value; }
  uint32_t get_uint32() const { return uint32_; }

  void set_uint64(uint64_t value) { uint64_ = value; }
  uint64_t get_uint64() const { return uint64_; }

  void set_string(std::string value) { string_ = value; }
  std::string_view get_string_as_string_piece() const { return string_; }
  const std::string& get_string() const { return string_; }

  const std::vector<std::string>& get_string_array() const {
    return string_array_;
  }
  std::vector<std::string>& get_mutable_string_array() { return string_array_; }

  const std::set<std::string>& get_string_set() const {
    return string_set_;
  }
  std::set<std::string>& get_mutable_string_set() { return string_set_; }

  const NestedStructWithTraitsImpl& get_struct() const { return struct_; }
  NestedStructWithTraitsImpl& get_mutable_struct() { return struct_; }

  const std::vector<NestedStructWithTraitsImpl>& get_struct_array() const {
    return struct_array_;
  }
  std::vector<NestedStructWithTraitsImpl>& get_mutable_struct_array() {
    return struct_array_;
  }

  const std::map<std::string, NestedStructWithTraitsImpl>& get_struct_map()
      const {
    return struct_map_;
  }
  std::map<std::string, NestedStructWithTraitsImpl>& get_mutable_struct_map() {
    return struct_map_;
  }

 private:
  EnumWithTraitsImpl enum_ = EnumWithTraitsImpl::CUSTOM_VALUE_0;
  bool bool_ = false;
  uint32_t uint32_ = 0;
  uint64_t uint64_ = 0;
  std::string string_;
  std::vector<std::string> string_array_;
  std::set<std::string> string_set_;
  NestedStructWithTraitsImpl struct_;
  std::vector<NestedStructWithTraitsImpl> struct_array_;
  std::map<std::string, NestedStructWithTraitsImpl> struct_map_;
};

// A type which corresponds nominally to the
// mojo::test::StructWithUnreachableTraits mojom type. Used to test that said
// type is never serialized, i.e. objects of this type are simply copied into
// a message as-is when written to an intra-process interface.
struct StructWithUnreachableTraitsImpl {
  int32_t magic_number = 0;
};

// A type which knows how to look like a mojo::test::TrivialStructWithTraits
// mojom type by way of mojo::StructTraits.
struct TrivialStructWithTraitsImpl {
  int32_t value;
};

// A type which knows how to look like a mojo::test::MoveOnlyStructWithTraits
// mojom type by way of mojo::StructTraits.
class MoveOnlyStructWithTraitsImpl {
 public:
  MoveOnlyStructWithTraitsImpl();
  MoveOnlyStructWithTraitsImpl(MoveOnlyStructWithTraitsImpl&& other);

  MoveOnlyStructWithTraitsImpl(const MoveOnlyStructWithTraitsImpl&) = delete;
  MoveOnlyStructWithTraitsImpl& operator=(const MoveOnlyStructWithTraitsImpl&) =
      delete;

  ~MoveOnlyStructWithTraitsImpl();

  ScopedHandle& get_mutable_handle() { return handle_; }

  MoveOnlyStructWithTraitsImpl& operator=(MoveOnlyStructWithTraitsImpl&& other);

 private:
  ScopedHandle handle_;
};

class UnionWithTraitsBase {
 public:
  enum class Type { INT32, STRUCT };

  virtual ~UnionWithTraitsBase() {}

  Type type() const { return type_; }

 protected:
  Type type_ = Type::INT32;
};

class UnionWithTraitsInt32 : public UnionWithTraitsBase {
 public:
  UnionWithTraitsInt32() {}
  explicit UnionWithTraitsInt32(int32_t value) : value_(value) {}

  ~UnionWithTraitsInt32() override;

  int32_t value() const { return value_; }
  void set_value(int32_t value) { value_ = value; }

 private:
  int32_t value_ = 0;
};

class UnionWithTraitsStruct : public UnionWithTraitsBase {
 public:
  UnionWithTraitsStruct() { type_ = Type::STRUCT; }
  explicit UnionWithTraitsStruct(int32_t value) : struct_(value) {
    type_ = Type::STRUCT;
  }
  ~UnionWithTraitsStruct() override;

  NestedStructWithTraitsImpl& get_mutable_struct() { return struct_; }
  const NestedStructWithTraitsImpl& get_struct() const { return struct_; }

 private:
  NestedStructWithTraitsImpl struct_;
};

class StructForceSerializeImpl {
 public:
  StructForceSerializeImpl();
  ~StructForceSerializeImpl();

  void set_value(int32_t value) { value_ = value; }
  int32_t value() const { return value_; }

  void set_was_serialized() const { was_serialized_ = true; }
  bool was_serialized() const { return was_serialized_; }

  void set_was_deserialized() { was_deserialized_ = true; }
  bool was_deserialized() const { return was_deserialized_; }

 private:
  int32_t value_ = 0;
  mutable bool was_serialized_ = false;
  bool was_deserialized_ = false;
};

class StructNestedForceSerializeImpl {
 public:
  StructNestedForceSerializeImpl();
  ~StructNestedForceSerializeImpl();

  StructForceSerializeImpl& force() { return force_; }
  const StructForceSerializeImpl& force() const { return force_; }

  void set_was_serialized() const { was_serialized_ = true; }
  bool was_serialized() const { return was_serialized_; }

  void set_was_deserialized() { was_deserialized_ = true; }
  bool was_deserialized() const { return was_deserialized_; }

 private:
  StructForceSerializeImpl force_;
  mutable bool was_serialized_ = false;
  bool was_deserialized_ = false;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_STRUCT_WITH_TRAITS_IMPL_H_
