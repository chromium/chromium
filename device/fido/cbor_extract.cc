// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/cbor_extract.h"

#include <type_traits>

#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/memory/raw_span.h"
#include "components/cbor/values.h"

namespace device {
namespace cbor_extract {

namespace {

using internal::Type;

static_assert(sizeof(StepOrByte<void>) == 1,
              "things should fit into a single byte");

const bool kTrue = true;
const bool kFalse = false;

constexpr uint8_t CBORTypeToBitfield(const cbor::Value::Type type) {
  const unsigned type_u = static_cast<unsigned>(type);
  if (type_u >= 8) {
    __builtin_unreachable();
  }
  return 1u << type_u;
}

// ASSERT_TYPE_IS asserts that the type of |a| is |b|. This is used to ensure
// that the documented output types for elements of |Type| are correct.
#define ASSERT_TYPE_IS(a, b)                                                \
  static_assert(                                                            \
      std::is_same<decltype(&a), decltype(reinterpret_cast<b*>(0))>::value, \
      "types need updating");

class Extractor {
 public:
  Extractor(base::span<const void*> outputs,
            base::span<const StepOrByte<void>> steps)
      : outputs_(outputs), steps_(steps) {}

  bool ValuesFromMap(const cbor::Value::MapValue& map) {
    for (;;) {
      const internal::Step step = steps_[step_i_++].step;
      const Type value_type = static_cast<Type>(step.value_type);
      if (value_type == Type::kStop) {
        return true;
      }

      const cbor::Value::MapValue::const_iterator map_it = map.find(NextKey());
      const void** output = nullptr;
      if (value_type != Type::kMap) {
        DCHECK_LT(step.output_index, outputs_.size());
        output = &outputs_[step.output_index];
      }

      if (map_it == map.end()) {
        if (step.required) {
          return false;
        }
        if (output) {
          *output = nullptr;
        }
        if (value_type == Type::kMap) {
          // When skipping an optional map, all the |StepOrByte| for the
          // elements of the map need to be skipped over.
          SeekPastNextStop();
        }
        continue;
      }

      // kExpectedCBORTypes is an array of bitmaps of acceptable types for each
      // |Type|.
      static constexpr uint8_t kExpectedCBORTypes[] = {
          // kBytestring
          CBORTypeToBitfield(cbor::Value::Type::BYTE_STRING),
          // kString
          CBORTypeToBitfield(cbor::Value::Type::STRING),
          // kBoolean
          CBORTypeToBitfield(cbor::Value::Type::SIMPLE_VALUE),
          // kInt
          CBORTypeToBitfield(cbor::Value::Type::NEGATIVE) |
              CBORTypeToBitfield(cbor::Value::Type::UNSIGNED),
          // kMap
          CBORTypeToBitfield(cbor::Value::Type::MAP),
          // kArray
          CBORTypeToBitfield(cbor::Value::Type::ARRAY),
          // kValue
          0xff,
      };

      const cbor::Value& value = map_it->second;
      const unsigned cbor_type_u = static_cast<unsigned>(value.type());
      const unsigned value_type_u = static_cast<unsigned>(value_type);
      DCHECK(value_type_u < std::size(kExpectedCBORTypes));
      if (cbor_type_u >= 8 ||
          (kExpectedCBORTypes[value_type_u] & (1u << cbor_type_u)) == 0) {
        return false;
      }

      switch (value_type) {
        case Type::kBytestring:
          ASSERT_TYPE_IS(value.GetBytestring(), const std::vector<uint8_t>);
          *output = &value.GetBytestring();
          break;
        case Type::kString:
          ASSERT_TYPE_IS(value.GetString(), const std::string);
          *output = &value.GetString();
          break;
        case Type::kBoolean:
          switch (value.GetSimpleValue()) {
            case cbor::Value::SimpleValue::TRUE_VALUE:
              *output = &kTrue;
              break;
            case cbor::Value::SimpleValue::FALSE_VALUE:
              *output = &kFalse;
              break;
            default:
              return false;
          }
          break;
        case Type::kInt:
          ASSERT_TYPE_IS(value.GetInteger(), const int64_t);
          *output = &value.GetInteger();
          break;
        case Type::kMap:
          if (!ValuesFromMap(value.GetMap())) {
            return false;
          }
          break;
        case Type::kArray:
          ASSERT_TYPE_IS(value.GetArray(), const std::vector<cbor::Value>);
          *output = &value.GetArray();
          break;
        case Type::kValue:
          *output = &value;
          break;
        case Type::kStop:
          return false;
      }
    }
  }

 private:
  // SeekPastNextStop increments |step_i_| until just after the next |Stop|
  // element, taking into account nested maps.
  void SeekPastNextStop() {
    for (;;) {
      const internal::Step step = steps_[step_i_++].step;
      const Type value_type = static_cast<Type>(step.value_type);
      if (value_type == Type::kStop) {
        break;
      }

      NextKey();

      if (value_type == Type::kMap) {
        SeekPastNextStop();
      } else {
        outputs_[step.output_index] = nullptr;
      }
    }
  }

  cbor::Value NextKey() {
    DCHECK_LT(step_i_, steps_.size());
    const uint8_t key_or_string_indicator = steps_[step_i_++].u8;
    if (key_or_string_indicator != StepOrByte<void>::STRING_KEY) {
      return cbor::Value(
          static_cast<int64_t>(static_cast<int8_t>(key_or_string_indicator)));
    }

    DCHECK_LT(step_i_, steps_.size());
    std::string key(&steps_[step_i_].c);
    step_i_ += key.size() + 1;
    DCHECK_LE(step_i_, steps_.size());
    return cbor::Value(std::move(key));
  }

  base::raw_span<const void*> outputs_;
  base::raw_span<const StepOrByte<void>> steps_;
  size_t step_i_ = 0;
};

}  // namespace

namespace internal {

bool Extract(base::span<const void*> outputs,
             base::span<const StepOrByte<void>> steps,
             const cbor::Value::MapValue& map) {
  DCHECK(steps[steps.size() - 1].step.value_type ==
         static_cast<uint8_t>(Type::kStop));
  Extractor extractor(outputs, steps);
  return extractor.ValuesFromMap(map);
}

}  // namespace internal

bool ForEachPublicKeyEntry(
    const cbor::Value::ArrayValue& array,
    const cbor::Value& key,
    base::RepeatingCallback<bool(const cbor::Value&)> callback) {
  const cbor::Value type_key("type");
  const std::string public_key("public-key");

  for (const cbor::Value& value : array) {
    if (!value.is_map()) {
      return false;
    }
    const cbor::Value::MapValue& map = value.GetMap();
    const auto type_it = map.find(type_key);
    if (type_it == map.end() || !type_it->second.is_string()) {
      return false;
    }
    if (type_it->second.GetString() != public_key) {
      continue;
    }

    const auto value_it = map.find(key);
    if (value_it == map.end() || !callback.Run(value_it->second)) {
      return false;
    }
  }

  return true;
}

}  // namespace cbor_extract
}  // namespace device
