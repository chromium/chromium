// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/mojo_optional_numerics_unittest.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content::optional_numerics_unittest {

// static
void Params::Bind(mojo::PendingReceiver<mojom::Params> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<Params>(), std::move(receiver));
}

void Params::SendNullBool(absl::optional<bool> optional_bool,
                          SendNullBoolCallback callback) {
  CHECK(!optional_bool.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint8(absl::optional<uint8_t> optional_uint8,
                           SendNullUint8Callback callback) {
  CHECK(!optional_uint8.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt8(absl::optional<int8_t> optional_int8,
                          SendNullInt8Callback callback) {
  CHECK(!optional_int8.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint16(absl::optional<uint16_t> optional_uint16,
                            SendNullUint16Callback callback) {
  CHECK(!optional_uint16.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt16(absl::optional<int16_t> optional_int16,
                           SendNullInt16Callback callback) {
  CHECK(!optional_int16.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint32(absl::optional<uint32_t> optional_uint32,
                            SendNullUint32Callback callback) {
  CHECK(!optional_uint32.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt32(absl::optional<int32_t> optional_int32,
                           SendNullInt32Callback callback) {
  CHECK(!optional_int32.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint64(absl::optional<uint64_t> optional_uint64,
                            SendNullUint64Callback callback) {
  CHECK(!optional_uint64.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt64(absl::optional<int64_t> optional_int64,
                           SendNullInt64Callback callback) {
  CHECK(!optional_int64.has_value());
  std::move(callback).Run();
}

void Params::SendNullFloat(absl::optional<float> optional_float,
                           SendNullFloatCallback callback) {
  CHECK(!optional_float.has_value());
  std::move(callback).Run();
}

void Params::SendNullDouble(absl::optional<double> optional_double,
                            SendNullDoubleCallback callback) {
  CHECK(!optional_double.has_value());
  std::move(callback).Run();
}

void Params::SendNullEnum(absl::optional<mojom::RegularEnum> optional_enum,
                          SendNullEnumCallback callback) {
  CHECK(!optional_enum.has_value());
  std::move(callback).Run();
}

void Params::SendOptionalBool(absl::optional<bool> optional_bool,
                              SendOptionalBoolCallback callback) {
  std::move(callback).Run(optional_bool.value());
}

void Params::SendOptionalUint8(absl::optional<uint8_t> optional_uint8,
                               SendOptionalUint8Callback callback) {
  std::move(callback).Run(optional_uint8.value());
}

void Params::SendOptionalInt8(absl::optional<int8_t> optional_int8,
                              SendOptionalInt8Callback callback) {
  std::move(callback).Run(optional_int8.value());
}

void Params::SendOptionalUint16(absl::optional<uint16_t> optional_uint16,
                                SendOptionalUint16Callback callback) {
  std::move(callback).Run(optional_uint16.value());
}

void Params::SendOptionalInt16(absl::optional<int16_t> optional_int16,
                               SendOptionalInt16Callback callback) {
  std::move(callback).Run(optional_int16.value());
}

void Params::SendOptionalUint32(absl::optional<uint32_t> optional_uint32,
                                SendOptionalUint32Callback callback) {
  std::move(callback).Run(optional_uint32.value());
}

void Params::SendOptionalInt32(absl::optional<int32_t> optional_int32,
                               SendOptionalInt32Callback callback) {
  std::move(callback).Run(optional_int32.value());
}

void Params::SendOptionalUint64(absl::optional<uint64_t> optional_uint64,
                                SendOptionalUint64Callback callback) {
  std::move(callback).Run(optional_uint64.value());
}

void Params::SendOptionalInt64(absl::optional<int64_t> optional_int64,
                               SendOptionalInt64Callback callback) {
  std::move(callback).Run(optional_int64.value());
}

void Params::SendOptionalFloat(absl::optional<float> optional_float,
                               SendOptionalFloatCallback callback) {
  std::move(callback).Run(optional_float.value());
}

void Params::SendOptionalDouble(absl::optional<double> optional_double,
                                SendOptionalDoubleCallback callback) {
  std::move(callback).Run(optional_double.value());
}

void Params::SendOptionalEnum(absl::optional<mojom::RegularEnum> optional_enum,
                              SendOptionalEnumCallback callback) {
  std::move(callback).Run(optional_enum.value());
}

void Params::SendNullStructWithOptionalNumerics(
    mojom::OptionalNumericsStructPtr s,
    SendNullStructWithOptionalNumericsCallback callback) {
  CHECK(s.is_null());
  std::move(callback).Run();
}

void Params::SendStructWithNullOptionalNumerics(
    mojom::OptionalNumericsStructPtr s,
    SendStructWithNullOptionalNumericsCallback callback) {
  CHECK(!s->optional_bool.has_value());
  CHECK(!s->optional_uint8.has_value());
  CHECK(!s->optional_int8.has_value());
  CHECK(!s->optional_uint16.has_value());
  CHECK(!s->optional_int16.has_value());
  CHECK(!s->optional_uint32.has_value());
  CHECK(!s->optional_int32.has_value());
  CHECK(!s->optional_uint64.has_value());
  CHECK(!s->optional_int64.has_value());
  CHECK(!s->optional_float.has_value());
  CHECK(!s->optional_double.has_value());
  CHECK(!s->optional_enum.has_value());
  std::move(callback).Run();
}

void Params::SendStructWithOptionalNumerics(
    mojom::OptionalNumericsStructPtr s,
    SendStructWithOptionalNumericsCallback callback) {
  std::move(callback).Run(s->optional_bool.value(), s->optional_uint8.value(),
                          s->optional_int8.value(), s->optional_uint16.value(),
                          s->optional_int16.value(), s->optional_uint32.value(),
                          s->optional_int32.value(), s->optional_uint64.value(),
                          s->optional_int64.value(), s->optional_float.value(),
                          s->optional_double.value(), s->optional_enum.value());
}

// static
void ResponseParams::Bind(
    mojo::PendingReceiver<mojom::ResponseParams> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<ResponseParams>(),
                              std::move(receiver));
}

void ResponseParams::GetNullBool(GetNullBoolCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullUint8(GetNullUint8Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullInt8(GetNullInt8Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullUint16(GetNullUint16Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullInt16(GetNullInt16Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullUint32(GetNullUint32Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullInt32(GetNullInt32Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullUint64(GetNullUint64Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullInt64(GetNullInt64Callback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullFloat(GetNullFloatCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullDouble(GetNullDoubleCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetNullEnum(GetNullEnumCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void ResponseParams::GetOptionalBool(bool value,
                                     GetOptionalBoolCallback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalUint8(uint8_t value,
                                      GetOptionalUint8Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalInt8(int8_t value,
                                     GetOptionalInt8Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalUint16(uint16_t value,
                                       GetOptionalUint16Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalInt16(int16_t value,
                                      GetOptionalInt16Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalUint32(uint32_t value,
                                       GetOptionalUint32Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalInt32(int32_t value,
                                      GetOptionalInt32Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalUint64(uint64_t value,
                                       GetOptionalUint64Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalInt64(int64_t value,
                                      GetOptionalInt64Callback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalFloat(float value,
                                      GetOptionalFloatCallback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalDouble(double value,
                                       GetOptionalDoubleCallback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetOptionalEnum(mojom::RegularEnum value,
                                     GetOptionalEnumCallback callback) {
  std::move(callback).Run(value);
}

void ResponseParams::GetNullStructWithOptionalNumerics(
    GetNullStructWithOptionalNumericsCallback callback) {
  std::move(callback).Run(nullptr);
}

void ResponseParams::GetStructWithNullOptionalNumerics(
    GetStructWithNullOptionalNumericsCallback callback) {
  std::move(callback).Run(mojom::OptionalNumericsStruct::New());
}

void ResponseParams::GetStructWithOptionalNumerics(
    bool bool_value,
    uint8_t uint8_value,
    int8_t int8_value,
    uint16_t uint16_value,
    int16_t int16_value,
    uint32_t uint32_value,
    int32_t int32_value,
    uint64_t uint64_value,
    int64_t int64_value,
    float float_value,
    double double_value,
    mojom::RegularEnum enum_value,
    GetStructWithOptionalNumericsCallback callback) {
  auto s = mojom::OptionalNumericsStruct::New(
      bool_value, uint8_value, int8_value, uint16_value, int16_value,
      uint32_value, int32_value, uint64_value, int64_value, float_value,
      double_value, enum_value);
  std::move(callback).Run(std::move(s));
}

}  // namespace content::optional_numerics_unittest
