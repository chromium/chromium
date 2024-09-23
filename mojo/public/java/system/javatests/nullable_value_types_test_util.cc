// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/android/jni_android.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/tests/nullable_value_types_enums.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/interfaces/bindings/tests/nullable_value_types.mojom.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "mojo/public/java/system/mojo_javatests_jni/NullableValueTypesTestUtil_jni.h"

namespace mojo {
namespace test::nullable_value_types {
namespace {

class InterfaceV2 : public mojom::InterfaceV2 {
  void MethodWithEnums(std::optional<mojom::RegularEnum> enum_value,
                       std::optional<TypemappedEnum> mapped_enum_value,
                       MethodWithEnumsCallback reply) override {
    std::move(reply).Run(enum_value, mapped_enum_value);
  }

  void MethodWithStructWithEnums(
      mojom::StructWithEnumsPtr in,
      MethodWithStructWithEnumsCallback reply) override {
    std::move(reply).Run(std::move(in));
  }

  void MethodWithNumerics(std::optional<bool> bool_value,
                          std::optional<uint8_t> u8_value,
                          std::optional<uint16_t> u16_value,
                          std::optional<uint32_t> u32_value,
                          std::optional<uint64_t> u64_value,
                          std::optional<int8_t> i8_value,
                          std::optional<int16_t> i16_value,
                          std::optional<int32_t> i32_value,
                          std::optional<int64_t> i64_value,
                          std::optional<float> float_value,
                          std::optional<double> double_value,
                          MethodWithNumericsCallback reply) override {
    std::move(reply).Run(bool_value, u8_value, u16_value, u32_value, u64_value,
                         i8_value, i16_value, i32_value, i64_value, float_value,
                         double_value);
  }

  void MethodWithStructWithNumerics(
      mojom::StructWithNumericsPtr in,
      MethodWithStructWithNumericsCallback reply) override {
    std::move(reply).Run(std::move(in));
  }

  void MethodWithVersionedArgs(std::optional<bool> bool_value,
                               std::optional<uint8_t> u8_value,
                               std::optional<uint16_t> u16_value,
                               std::optional<uint32_t> u32_value,
                               std::optional<uint64_t> u64_value,
                               std::optional<int8_t> i8_value,
                               std::optional<int16_t> i16_value,
                               std::optional<int32_t> i32_value,
                               std::optional<int64_t> i64_value,
                               std::optional<float> float_value,
                               std::optional<double> double_value,
                               std::optional<mojom::RegularEnum> enum_value,
                               std::optional<TypemappedEnum> mapped_enum_value,
                               MethodWithVersionedArgsCallback reply) override {
    // Not currently exercised by tests.
    NOTREACHED();
  }

  void MethodWithVersionedStruct(
      mojom::VersionedStructV2Ptr in,
      MethodWithVersionedStructCallback reply) override {
    // Not currently exercised by tests.
    NOTREACHED();
  }

  void MethodWithContainers(
      const std::vector<std::optional<bool>>& bool_values,
      const std::vector<std::optional<uint8_t>>& u8_values,
      const std::vector<std::optional<uint16_t>>& u16_values,
      const std::vector<std::optional<uint32_t>>& u32_values,
      const std::vector<std::optional<uint64_t>>& u64_values,
      const std::vector<std::optional<int8_t>>& i8_values,
      const std::vector<std::optional<int16_t>>& i16_values,
      const std::vector<std::optional<int32_t>>& i32_values,
      const std::vector<std::optional<int64_t>>& i64_values,
      const std::vector<std::optional<float>>& float_values,
      const std::vector<std::optional<double>>& double_values,
      const std::vector<std::optional<mojom::RegularEnum>>& enum_values,
      const std::vector<std::optional<mojom::ExtensibleEnum>>&
          extensible_enum_values,
      const base::flat_map<int32_t, std::optional<bool>>& bool_map,
      const base::flat_map<int32_t, std::optional<int32_t>>& int_map,
      MethodWithContainersCallback reply) override {
    std::move(reply).Run(bool_values, u8_values, u16_values, u32_values,
                         u64_values, i8_values, i16_values, i32_values,
                         i64_values, float_values, double_values, enum_values,
                         extensible_enum_values, bool_map, int_map);
  }

  void MethodToSendUnknownEnum(MethodToSendUnknownEnumCallback reply) override {
    std::move(reply).Run(std::vector<std::optional<mojom::ExtensibleEnum>>{
        static_cast<mojom::ExtensibleEnum>(
            555),  // intentionally an unknown enum value.
        std::nullopt});
  }
};

}  // namespace
}  // namespace test::nullable_value_types

namespace android {

void JNI_NullableValueTypesTestUtil_BindTestInterface(
    JNIEnv* env,
    jlong raw_message_pipe_handle) {
  mojo::PendingReceiver<test::nullable_value_types::mojom::InterfaceV2>
      pending_receiver{mojo::ScopedMessagePipeHandle(
          mojo::MessagePipeHandle(raw_message_pipe_handle))};
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<test::nullable_value_types::InterfaceV2>(),
      std::move(pending_receiver));
}

}  // namespace android
}  // namespace mojo
