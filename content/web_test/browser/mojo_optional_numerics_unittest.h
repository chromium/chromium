// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_MOJO_OPTIONAL_NUMERICS_UNITTEST_H_
#define CONTENT_WEB_TEST_BROWSER_MOJO_OPTIONAL_NUMERICS_UNITTEST_H_

#include "content/web_test/common/mojo_optional_numerics_unittest.mojom.h"

namespace content::optional_numerics_unittest {

class Params : public mojom::Params {
 public:
  static void Bind(mojo::PendingReceiver<mojom::Params> receiver);

  void SendNullBool(absl::optional<bool> optional_bool,
                    SendNullBoolCallback callback) override;
  void SendNullUint8(absl::optional<uint8_t> optional_uint8,
                     SendNullUint8Callback callback) override;
  void SendNullInt8(absl::optional<int8_t> optional_int8,
                    SendNullInt8Callback callback) override;
  void SendNullUint16(absl::optional<uint16_t> optional_uint16,
                      SendNullUint16Callback callback) override;
  void SendNullInt16(absl::optional<int16_t> optional_int16,
                     SendNullInt16Callback callback) override;
  void SendNullUint32(absl::optional<uint32_t> optional_uint32,
                      SendNullUint32Callback callback) override;
  void SendNullInt32(absl::optional<int32_t> optional_int32,
                     SendNullInt32Callback callback) override;
  void SendNullUint64(absl::optional<uint64_t> optional_uint64,
                      SendNullUint64Callback callback) override;
  void SendNullInt64(absl::optional<int64_t> optional_int64,
                     SendNullInt64Callback callback) override;
  void SendNullFloat(absl::optional<float> optional_float,
                     SendNullFloatCallback callback) override;
  void SendNullDouble(absl::optional<double> optional_double,
                      SendNullDoubleCallback callback) override;
  void SendNullEnum(absl::optional<mojom::RegularEnum> optional_enum,
                    SendNullEnumCallback callback) override;

  void SendOptionalBool(absl::optional<bool> optional_bool,
                        SendOptionalBoolCallback callback) override;
  void SendOptionalUint8(absl::optional<uint8_t> optional_uint8,
                         SendOptionalUint8Callback callback) override;
  void SendOptionalInt8(absl::optional<int8_t> optional_int8,
                        SendOptionalInt8Callback callback) override;
  void SendOptionalUint16(absl::optional<uint16_t> optional_uint16,
                          SendOptionalUint16Callback callback) override;
  void SendOptionalInt16(absl::optional<int16_t> optional_int16,
                         SendOptionalInt16Callback callback) override;
  void SendOptionalUint32(absl::optional<uint32_t> optional_uint32,
                          SendOptionalUint32Callback callback) override;
  void SendOptionalInt32(absl::optional<int32_t> optional_int32,
                         SendOptionalInt32Callback callback) override;
  void SendOptionalUint64(absl::optional<uint64_t> optional_uint64,
                          SendOptionalUint64Callback callback) override;
  void SendOptionalInt64(absl::optional<int64_t> optional_int64,
                         SendOptionalInt64Callback callback) override;
  void SendOptionalFloat(absl::optional<float> optional_float,
                         SendOptionalFloatCallback callback) override;
  void SendOptionalDouble(absl::optional<double> optional_double,
                          SendOptionalDoubleCallback callback) override;
  void SendOptionalEnum(absl::optional<mojom::RegularEnum> optional_enum,
                        SendOptionalEnumCallback callback) override;

  void SendNullStructWithOptionalNumerics(
      mojom::OptionalNumericsStructPtr s,
      SendNullStructWithOptionalNumericsCallback callback) override;
  void SendStructWithNullOptionalNumerics(
      mojom::OptionalNumericsStructPtr s,
      SendStructWithNullOptionalNumericsCallback callback) override;
  void SendStructWithOptionalNumerics(
      mojom::OptionalNumericsStructPtr s,
      SendStructWithOptionalNumericsCallback callback) override;
};

class ResponseParams : public mojom::ResponseParams {
 public:
  static void Bind(mojo::PendingReceiver<mojom::ResponseParams> receiver);

  void GetNullBool(GetNullBoolCallback callback) override;

  void GetNullUint8(GetNullUint8Callback callback) override;
  void GetNullInt8(GetNullInt8Callback callback) override;
  void GetNullUint16(GetNullUint16Callback callback) override;
  void GetNullInt16(GetNullInt16Callback callback) override;
  void GetNullUint32(GetNullUint32Callback callback) override;
  void GetNullInt32(GetNullInt32Callback callback) override;
  void GetNullUint64(GetNullUint64Callback callback) override;
  void GetNullInt64(GetNullInt64Callback callback) override;
  void GetNullFloat(GetNullFloatCallback callback) override;
  void GetNullDouble(GetNullDoubleCallback callback) override;
  void GetNullEnum(GetNullEnumCallback callback) override;

  void GetOptionalBool(bool value, GetOptionalBoolCallback callback) override;
  void GetOptionalUint8(uint8_t value,
                        GetOptionalUint8Callback callback) override;
  void GetOptionalInt8(int8_t value, GetOptionalInt8Callback callback) override;
  void GetOptionalUint16(uint16_t value,
                         GetOptionalUint16Callback callback) override;
  void GetOptionalInt16(int16_t value,
                        GetOptionalInt16Callback callback) override;
  void GetOptionalUint32(uint32_t value,
                         GetOptionalUint32Callback callback) override;
  void GetOptionalInt32(int32_t value,
                        GetOptionalInt32Callback callback) override;
  void GetOptionalUint64(uint64_t value,
                         GetOptionalUint64Callback callback) override;
  void GetOptionalInt64(int64_t value,
                        GetOptionalInt64Callback callback) override;
  void GetOptionalFloat(float value,
                        GetOptionalFloatCallback callback) override;
  void GetOptionalDouble(double value,
                         GetOptionalDoubleCallback callback) override;
  void GetOptionalEnum(mojom::RegularEnum value,
                       GetOptionalEnumCallback callback) override;

  void GetNullStructWithOptionalNumerics(
      GetNullStructWithOptionalNumericsCallback callback) override;
  void GetStructWithNullOptionalNumerics(
      GetStructWithNullOptionalNumericsCallback callback) override;
  void GetStructWithOptionalNumerics(
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
      GetStructWithOptionalNumericsCallback callback) override;
};

}  // namespace content::optional_numerics_unittest

#endif  // CONTENT_WEB_TEST_BROWSER_MOJO_OPTIONAL_NUMERICS_UNITTEST_H_
