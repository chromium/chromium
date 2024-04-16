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

void Params::SendNullBool(std::optional<bool> optional_bool,
                          SendNullBoolCallback callback) {
  CHECK(!optional_bool.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint8(std::optional<uint8_t> optional_uint8,
                           SendNullUint8Callback callback) {
  CHECK(!optional_uint8.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt8(std::optional<int8_t> optional_int8,
                          SendNullInt8Callback callback) {
  CHECK(!optional_int8.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint16(std::optional<uint16_t> optional_uint16,
                            SendNullUint16Callback callback) {
  CHECK(!optional_uint16.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt16(std::optional<int16_t> optional_int16,
                           SendNullInt16Callback callback) {
  CHECK(!optional_int16.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint32(std::optional<uint32_t> optional_uint32,
                            SendNullUint32Callback callback) {
  CHECK(!optional_uint32.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt32(std::optional<int32_t> optional_int32,
                           SendNullInt32Callback callback) {
  CHECK(!optional_int32.has_value());
  std::move(callback).Run();
}

void Params::SendNullUint64(std::optional<uint64_t> optional_uint64,
                            SendNullUint64Callback callback) {
  CHECK(!optional_uint64.has_value());
  std::move(callback).Run();
}

void Params::SendNullInt64(std::optional<int64_t> optional_int64,
                           SendNullInt64Callback callback) {
  CHECK(!optional_int64.has_value());
  std::move(callback).Run();
}

void Params::SendNullFloat(std::optional<float> optional_float,
                           SendNullFloatCallback callback) {
  CHECK(!optional_float.has_value());
  std::move(callback).Run();
}

void Params::SendNullDouble(std::optional<double> optional_double,
                            SendNullDoubleCallback callback) {
  CHECK(!optional_double.has_value());
  std::move(callback).Run();
}

void Params::SendNullEnum(std::optional<mojom::RegularEnum> optional_enum,
                          SendNullEnumCallback callback) {
  CHECK(!optional_enum.has_value());
  std::move(callback).Run();
}

template <typename T>
void CheckAllNulls(const std::vector<T>& optionals) {
  for (auto& optional : optionals) {
    CHECK(!optional.has_value());
  }
}

void Params::SendNullBools(
    const std::vector<std::optional<bool>>& optional_bools,
    SendNullBoolsCallback callback) {
  CheckAllNulls(optional_bools);
  std::move(callback).Run();
}

void Params::SendNullInt16s(
    const std::vector<std::optional<int16_t>>& optional_int16s,
    SendNullInt16sCallback callback) {
  CheckAllNulls(optional_int16s);
  std::move(callback).Run();
}

void Params::SendNullUint32s(
    const std::vector<std::optional<uint32_t>>& optional_uint32s,
    SendNullUint32sCallback callback) {
  CheckAllNulls(optional_uint32s);
  std::move(callback).Run();
}

void Params::SendNullDoubles(
    const std::vector<std::optional<double>>& optional_doubles,
    SendNullDoublesCallback callback) {
  CheckAllNulls(optional_doubles);
  std::move(callback).Run();
}

void Params::SendNullEnums(
    const std::vector<std::optional<mojom::RegularEnum>>& optional_enums,
    SendNullEnumsCallback callback) {
  CheckAllNulls(optional_enums);
  std::move(callback).Run();
}

template <typename K, typename V>
void CheckAllNulls(const base::flat_map<K, V>& values) {
  for (auto& pair : values) {
    CHECK(!pair.second.has_value());
  }
}

void Params::SendNullBoolMap(
    const base::flat_map<int32_t, std::optional<bool>>& values,
    SendNullBoolMapCallback callback) {
  CheckAllNulls(values);
  std::move(callback).Run();
}

void Params::SendNullDoubleMap(
    const base::flat_map<int32_t, std::optional<double>>& values,
    SendNullDoubleMapCallback callback) {
  CheckAllNulls(values);
  std::move(callback).Run();
}

void Params::SendNullEnumMap(
    const base::flat_map<int32_t, std::optional<mojom::RegularEnum>>& values,
    SendNullEnumMapCallback callback) {
  CheckAllNulls(values);
  std::move(callback).Run();
}

void Params::SendOptionalBool(std::optional<bool> optional_bool,
                              SendOptionalBoolCallback callback) {
  std::move(callback).Run(optional_bool.value());
}

void Params::SendOptionalUint8(std::optional<uint8_t> optional_uint8,
                               SendOptionalUint8Callback callback) {
  std::move(callback).Run(optional_uint8.value());
}

void Params::SendOptionalInt8(std::optional<int8_t> optional_int8,
                              SendOptionalInt8Callback callback) {
  std::move(callback).Run(optional_int8.value());
}

void Params::SendOptionalUint16(std::optional<uint16_t> optional_uint16,
                                SendOptionalUint16Callback callback) {
  std::move(callback).Run(optional_uint16.value());
}

void Params::SendOptionalInt16(std::optional<int16_t> optional_int16,
                               SendOptionalInt16Callback callback) {
  std::move(callback).Run(optional_int16.value());
}

void Params::SendOptionalUint32(std::optional<uint32_t> optional_uint32,
                                SendOptionalUint32Callback callback) {
  std::move(callback).Run(optional_uint32.value());
}

void Params::SendOptionalInt32(std::optional<int32_t> optional_int32,
                               SendOptionalInt32Callback callback) {
  std::move(callback).Run(optional_int32.value());
}

void Params::SendOptionalUint64(std::optional<uint64_t> optional_uint64,
                                SendOptionalUint64Callback callback) {
  std::move(callback).Run(optional_uint64.value());
}

void Params::SendOptionalInt64(std::optional<int64_t> optional_int64,
                               SendOptionalInt64Callback callback) {
  std::move(callback).Run(optional_int64.value());
}

void Params::SendOptionalFloat(std::optional<float> optional_float,
                               SendOptionalFloatCallback callback) {
  std::move(callback).Run(optional_float.value());
}

void Params::SendOptionalDouble(std::optional<double> optional_double,
                                SendOptionalDoubleCallback callback) {
  std::move(callback).Run(optional_double.value());
}

void Params::SendOptionalEnum(std::optional<mojom::RegularEnum> optional_enum,
                              SendOptionalEnumCallback callback) {
  std::move(callback).Run(optional_enum.value());
}

template <typename T>
std::vector<T> Unwrap(const std::vector<std::optional<T>>& in) {
  std::vector<T> out;
  for (auto e : in) {
    if (e) {
      out.push_back(*e);
    }
  }
  return out;
}

void Params ::SendOptionalBools(
    const std::vector<std::optional<bool>>& optional_bools,
    SendOptionalBoolsCallback callback) {
  std::move(callback).Run(Unwrap(optional_bools));
}

void Params ::SendOptionalInt16s(
    const std::vector<std::optional<int16_t>>& optional_int16s,
    SendOptionalInt16sCallback callback) {
  std::move(callback).Run(Unwrap(optional_int16s));
}

void Params ::SendOptionalUint32s(
    const std::vector<std::optional<uint32_t>>& optional_uint32s,
    SendOptionalUint32sCallback callback) {
  std::move(callback).Run(Unwrap(optional_uint32s));
}

void Params ::SendOptionalDoubles(
    const std::vector<std::optional<double>>& optional_doubles,
    SendOptionalDoublesCallback callback) {
  std::move(callback).Run(Unwrap(optional_doubles));
}

void Params ::SendOptionalEnums(
    const std::vector<std::optional<mojom::RegularEnum>>& optional_enums,
    SendOptionalEnumsCallback callback) {
  std::move(callback).Run(Unwrap(optional_enums));
}

template <typename K, typename V>
base::flat_map<K, V> Unwrap(const base::flat_map<K, std::optional<V>>& values) {
  base::flat_map<K, V> out;
  for (const auto& entry : values) {
    if (entry.second) {
      out.insert({entry.first, *entry.second});
    }
  }
  return out;
}

void Params::SendOptionalBoolMap(
    const base::flat_map<int32_t, std::optional<bool>>& values,
    SendOptionalBoolMapCallback callback) {
  std::move(callback).Run(Unwrap(values));
}

void Params::SendOptionalDoubleMap(
    const base::flat_map<int32_t, std::optional<double>>& values,
    SendOptionalDoubleMapCallback callback) {
  std::move(callback).Run(Unwrap(values));
}

void Params::SendOptionalEnumMap(
    const base::flat_map<int32_t, std::optional<mojom::RegularEnum>>& values,
    SendOptionalEnumMapCallback callback) {
  std::move(callback).Run(Unwrap(values));
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
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullUint8(GetNullUint8Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullInt8(GetNullInt8Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullUint16(GetNullUint16Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullInt16(GetNullInt16Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullUint32(GetNullUint32Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullInt32(GetNullInt32Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullUint64(GetNullUint64Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullInt64(GetNullInt64Callback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullFloat(GetNullFloatCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullDouble(GetNullDoubleCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void ResponseParams::GetNullEnum(GetNullEnumCallback callback) {
  std::move(callback).Run(std::nullopt);
}
void ResponseParams::GetNullBools(GetNullBoolsCallback callback) {
  std::move(callback).Run(std::vector({std::optional<bool>()}));
}

void ResponseParams::GetNullInt16s(GetNullInt16sCallback callback) {
  std::move(callback).Run(std::vector({std::optional<int16_t>()}));
}

void ResponseParams::GetNullUint32s(GetNullUint32sCallback callback) {
  std::move(callback).Run(std::vector({std::optional<uint32_t>()}));
}

void ResponseParams::GetNullDoubles(GetNullDoublesCallback callback) {
  std::move(callback).Run(std::vector({std::optional<double>()}));
}

void ResponseParams::GetNullEnums(GetNullEnumsCallback callback) {
  std::move(callback).Run(std::vector({std::optional<mojom::RegularEnum>()}));
}

void ResponseParams::GetNullBoolMap(GetNullBoolMapCallback callback) {
  std::move(callback).Run(
      base::flat_map<int16_t, std::optional<bool>>({{0, std::nullopt}}));
}

void ResponseParams::GetNullInt32Map(GetNullInt32MapCallback callback) {
  std::move(callback).Run(
      base::flat_map<int16_t, std::optional<int32_t>>({{0, std::nullopt}}));
}

void ResponseParams::GetNullEnumMap(GetNullEnumMapCallback callback) {
  std::move(callback).Run(
      base::flat_map<int16_t, std::optional<mojom::RegularEnum>>(
          {{0, std::nullopt}}));
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

template <typename T>
std::vector<std::optional<T>> WrapWithNulls(T value) {
  return std::vector<std::optional<T>>({std::nullopt, value, std::nullopt});
}

void ResponseParams::GetOptionalBools(bool value,
                                      GetOptionalBoolsCallback callback) {
  std::move(callback).Run(WrapWithNulls(value));
}
void ResponseParams::GetOptionalInt16s(int16_t value,
                                       GetOptionalInt16sCallback callback) {
  std::move(callback).Run(WrapWithNulls(value));
}
void ResponseParams::GetOptionalUint32s(uint32_t value,
                                        GetOptionalUint32sCallback callback) {
  std::move(callback).Run(WrapWithNulls(value));
}
void ResponseParams::GetOptionalDoubles(double value,
                                        GetOptionalDoublesCallback callback) {
  std::move(callback).Run(WrapWithNulls(value));
}
void ResponseParams::GetOptionalEnums(mojom::RegularEnum value,
                                      GetOptionalEnumsCallback callback) {
  std::move(callback).Run(WrapWithNulls(value));
}

template <typename K, typename V>
base::flat_map<K, std::optional<V>> WrapWithNulls(K key, V value) {
  return base::flat_map<K, std::optional<V>>(
      {{key - 1, std::nullopt}, {key, value}, {key + 1, std::nullopt}});
}

void ResponseParams::GetOptionalBoolMap(int16_t key,
                                        bool value,
                                        GetOptionalBoolMapCallback callback) {
  std::move(callback).Run(WrapWithNulls(key, value));
}
void ResponseParams::GetOptionalFloatMap(int16_t key,
                                         float value,
                                         GetOptionalFloatMapCallback callback) {
  std::move(callback).Run(WrapWithNulls(key, value));
}
void ResponseParams::GetOptionalEnumMap(int16_t key,
                                        mojom::RegularEnum value,
                                        GetOptionalEnumMapCallback callback) {
  std::move(callback).Run(WrapWithNulls(key, value));
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
