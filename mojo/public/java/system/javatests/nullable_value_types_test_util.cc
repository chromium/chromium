// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/android/jni_android.h"
#include "base/notreached.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/tests/nullable_value_types_enums.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/interfaces/bindings/tests/nullable_value_types.mojom.h"
#include "mojo/public/java/system/mojo_javatests_jni/NullableValueTypesTestUtil_jni.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {
namespace test::nullable_value_types {
namespace {

class InterfaceV2 : public mojom::InterfaceV2 {
  void MethodWithEnums(absl::optional<mojom::RegularEnum> enum_value,
                       absl::optional<TypemappedEnum> mapped_enum_value,
                       MethodWithEnumsCallback reply) override {
    std::move(reply).Run(enum_value, mapped_enum_value);
  }

  void MethodWithStructWithEnums(
      mojom::StructWithEnumsPtr in,
      MethodWithStructWithEnumsCallback reply) override {
    std::move(reply).Run(std::move(in));
  }

  void MethodWithNumerics(absl::optional<bool> bool_value,
                          absl::optional<uint8_t> u8_value,
                          absl::optional<uint16_t> u16_value,
                          absl::optional<uint32_t> u32_value,
                          absl::optional<uint64_t> u64_value,
                          absl::optional<int8_t> i8_value,
                          absl::optional<int16_t> i16_value,
                          absl::optional<int32_t> i32_value,
                          absl::optional<int64_t> i64_value,
                          absl::optional<float> float_value,
                          absl::optional<double> double_value,
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

  void MethodWithVersionedArgs(absl::optional<bool> bool_value,
                               absl::optional<uint8_t> u8_value,
                               absl::optional<uint16_t> u16_value,
                               absl::optional<uint32_t> u32_value,
                               absl::optional<uint64_t> u64_value,
                               absl::optional<int8_t> i8_value,
                               absl::optional<int16_t> i16_value,
                               absl::optional<int32_t> i32_value,
                               absl::optional<int64_t> i64_value,
                               absl::optional<float> float_value,
                               absl::optional<double> double_value,
                               absl::optional<mojom::RegularEnum> enum_value,
                               absl::optional<TypemappedEnum> mapped_enum_value,
                               MethodWithVersionedArgsCallback reply) override {
    // Not currently exercised by tests.
    NOTREACHED_NORETURN();
  }

  void MethodWithVersionedStruct(
      mojom::VersionedStructV2Ptr in,
      MethodWithVersionedStructCallback reply) override {
    // Not currently exercised by tests.
    NOTREACHED_NORETURN();
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
