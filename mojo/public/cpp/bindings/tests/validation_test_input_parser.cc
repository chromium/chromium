// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/c/system/macros.h"

namespace mojo {
namespace test {
namespace {

class ValidationTestInputParser {
 public:
  ValidationTestInputParser(const std::string& input,
                            std::vector<uint8_t>* data,
                            size_t* num_handles,
                            std::string* error_message);
  ~ValidationTestInputParser();

  bool Run();

 private:
  struct DataType;

  typedef bool (ValidationTestInputParser::*ParseDataFunc)(
      const DataType& type,
      std::string_view value_string);

  struct DataType {
    std::string_view name;
    size_t data_size;
    ParseDataFunc parse_data_func;
  };

  // A dist4/8 item that hasn't been matched with an anchr item.
  struct PendingDistanceItem {
    // Where this data item is located in |data_|.
    size_t pos;
    // Either 4 or 8 (bytes).
    size_t data_size;
  };

  bool GetNextItem(std::string_view* item);

  bool ParseItem(std::string_view item);

  bool ParseUnsignedInteger(const DataType& type,
                            std::string_view value_string);
  bool ParseSignedInteger(const DataType& type, std::string_view value_string);
  bool ParseFloatingPoint(const DataType& type, std::string_view value_string);
  bool ParseBinarySequence(const DataType& type, std::string_view value_string);
  bool ParseDistance(const DataType& type, std::string_view value_string);
  bool ParseAnchor(const DataType& type, std::string_view value_string);
  bool ParseHandles(const DataType& type, std::string_view value_string);

  bool ConvertToUnsignedInteger(std::string_view value_string, uint64_t* value);
  bool ConvertToSignedInteger(std::string_view value_string, int64_t* value);

  template <typename T>
  void AppendData(T data) {
    for (const uint8_t& byte : AsBytes(data)) {
      data_->push_back(byte);
    }
  }

  template <typename TargetType, typename InputType>
  bool ConvertAndAppendData(InputType value) {
    if (value > std::numeric_limits<TargetType>::max() ||
        value < std::numeric_limits<TargetType>::min()) {
      return false;
    }
    AppendData(static_cast<TargetType>(value));
    return true;
  }

  template <typename TargetType, typename InputType>
  bool ConvertAndFillData(size_t pos, InputType value) {
    if (value > std::numeric_limits<TargetType>::max() ||
        value < std::numeric_limits<TargetType>::min()) {
      return false;
    }
    TargetType target_value = static_cast<TargetType>(value);
    assert(pos + sizeof(TargetType) <= data_->size());
    for (const uint8_t& byte : AsBytes(target_value)) {
      (*data_)[pos++] = byte;
    }
    return true;
  }

  template <typename T>
  static base::span<const uint8_t, sizeof(T)> AsBytes(
      const T& data LIFETIME_BOUND) {
    if constexpr (std::has_unique_object_representations_v<T>) {
      return base::byte_span_from_ref(data);
    } else {
      return base::byte_span_from_ref(base::allow_nonunique_obj, data);
    }
  }

  static const std::array<DataType, 15> kDataTypes;

  const raw_ref<const std::string> input_;
  const std::string_view input_view_;
  size_t input_cursor_ = 0;

  raw_ptr<std::vector<uint8_t>> data_;
  raw_ptr<size_t> num_handles_;
  raw_ptr<std::string> error_message_;

  std::map<std::string_view, PendingDistanceItem> pending_distance_items_;
  std::set<std::string_view> anchors_;
};

const std::array<ValidationTestInputParser::DataType, 15>
    ValidationTestInputParser::kDataTypes = {
        {{"[u1]", 1, &ValidationTestInputParser::ParseUnsignedInteger},
         {"[u2]", 2, &ValidationTestInputParser::ParseUnsignedInteger},
         {"[u4]", 4, &ValidationTestInputParser::ParseUnsignedInteger},
         {"[u8]", 8, &ValidationTestInputParser::ParseUnsignedInteger},
         {"[s1]", 1, &ValidationTestInputParser::ParseSignedInteger},
         {"[s2]", 2, &ValidationTestInputParser::ParseSignedInteger},
         {"[s4]", 4, &ValidationTestInputParser::ParseSignedInteger},
         {"[s8]", 8, &ValidationTestInputParser::ParseSignedInteger},
         {"[b]", 1, &ValidationTestInputParser::ParseBinarySequence},
         {"[f]", 4, &ValidationTestInputParser::ParseFloatingPoint},
         {"[d]", 8, &ValidationTestInputParser::ParseFloatingPoint},
         {"[dist4]", 4, &ValidationTestInputParser::ParseDistance},
         {"[dist8]", 8, &ValidationTestInputParser::ParseDistance},
         {"[anchr]", 0, &ValidationTestInputParser::ParseAnchor},
         {"[handles]", 0, &ValidationTestInputParser::ParseHandles}}};

ValidationTestInputParser::ValidationTestInputParser(const std::string& input,
                                                     std::vector<uint8_t>* data,
                                                     size_t* num_handles,
                                                     std::string* error_message)
    : input_(input),
      input_view_(*input_),
      data_(data),
      num_handles_(num_handles),
      error_message_(error_message) {
  assert(data_);
  assert(num_handles_);
  assert(error_message_);
  data_->clear();
  *num_handles_ = 0;
  error_message_->clear();
}

ValidationTestInputParser::~ValidationTestInputParser() {
}

bool ValidationTestInputParser::Run() {
  std::string_view item;
  bool result = true;
  while (result && GetNextItem(&item)) {
    result = ParseItem(item);
  }

  if (!result) {
    *error_message_ = "Error occurred when parsing " + std::string(item);
  } else if (!pending_distance_items_.empty()) {
    // We have parsed all the contents in |input_| successfully, but there are
    // unmatched dist4/8 items.
    *error_message_ = "Error occurred when matching [dist4/8] and [anchr].";
    result = false;
  }
  if (!result) {
    data_->clear();
    *num_handles_ = 0;
  } else {
    assert(error_message_->empty());
  }

  return result;
}

bool ValidationTestInputParser::GetNextItem(std::string_view* item) {
  const char kWhitespaceChars[] = " \t\n\r";
  const char kItemDelimiters[] = " \t\n\r/";
  const char kEndOfLineChars[] = "\n\r";
  while (true) {
    // Skip leading whitespaces.
    // If there are no non-whitespace characters left, |input_cursor_| will be
    // set to std::string_view::npos.
    input_cursor_ =
        input_view_.find_first_not_of(kWhitespaceChars, input_cursor_);

    if (input_cursor_ == std::string_view::npos) {
      return false;
    }

    if (input_view_.substr(input_cursor_).starts_with("//")) {
      // Skip contents until the end of the line.
      input_cursor_ = input_view_.find_first_of(kEndOfLineChars, input_cursor_);
      continue;
    }

    size_t end_pos = input_view_.find_first_of(kItemDelimiters, input_cursor_);
    size_t count = std::string_view::npos;  // Character count of the item
    if (end_pos != std::string_view::npos) {
      count = end_pos - input_cursor_;
    }
    *item = input_view_.substr(input_cursor_, count);
    input_cursor_ = end_pos;
    return true;
  }
}

bool ValidationTestInputParser::ParseItem(std::string_view item) {
  for (const auto& data_type : kDataTypes) {
    if (item.starts_with(data_type.name)) {
      return (this->*data_type.parse_data_func)(
          data_type, item.substr(data_type.name.size()));
    }
  }

  // "[u1]" is optional.
  return ParseUnsignedInteger(kDataTypes.front(), item);
}

bool ValidationTestInputParser::ParseUnsignedInteger(
    const DataType& type,
    std::string_view value_string) {
  uint64_t value;
  if (!ConvertToUnsignedInteger(value_string, &value)) {
    return false;
  }

  switch (type.data_size) {
    case 1:
      return ConvertAndAppendData<uint8_t>(value);
    case 2:
      return ConvertAndAppendData<uint16_t>(value);
    case 4:
      return ConvertAndAppendData<uint32_t>(value);
    case 8:
      return ConvertAndAppendData<uint64_t>(value);
    default:
      assert(false);
      return false;
  }
}

bool ValidationTestInputParser::ParseSignedInteger(
    const DataType& type,
    std::string_view value_string) {
  int64_t value;
  if (!ConvertToSignedInteger(value_string, &value)) {
    return false;
  }

  switch (type.data_size) {
    case 1:
      return ConvertAndAppendData<int8_t>(value);
    case 2:
      return ConvertAndAppendData<int16_t>(value);
    case 4:
      return ConvertAndAppendData<int32_t>(value);
    case 8:
      return ConvertAndAppendData<int64_t>(value);
    default:
      assert(false);
      return false;
  }
}

bool ValidationTestInputParser::ParseFloatingPoint(
    const DataType& type,
    std::string_view value_string) {
  static_assert(sizeof(float) == 4, "sizeof(float) is not 4");
  static_assert(sizeof(double) == 8, "sizeof(double) is not 8");

  double value;
  if (!base::StringToDouble(value_string, &value)) {
    return false;
  }

  switch (type.data_size) {
    case 4:
      AppendData(static_cast<float>(value));
      return true;
    case 8:
      AppendData(value);
      return true;
    default:
      assert(false);
      return false;
  }
}

bool ValidationTestInputParser::ParseBinarySequence(
    const DataType& type,
    std::string_view value_string) {
  if (value_string.size() != 8)
    return false;

  uint8_t value = 0;
  for (const auto& c : value_string) {
    value <<= 1;
    if (c == '1') {
      value++;
    } else if (c != '0') {
      return false;
    }
  }
  AppendData(value);
  return true;
}

bool ValidationTestInputParser::ParseDistance(const DataType& type,
                                              std::string_view value_string) {
  if (base::Contains(pending_distance_items_, value_string)) {
    return false;
  }

  PendingDistanceItem item = {data_->size(), type.data_size};
  data_->resize(data_->size() + type.data_size);
  pending_distance_items_[value_string] = item;

  return true;
}

bool ValidationTestInputParser::ParseAnchor(const DataType& type,
                                            std::string_view value_string) {
  if (!anchors_.insert(value_string).second) {
    return false;
  }

  std::map<std::string_view, PendingDistanceItem>::const_iterator iter =
      pending_distance_items_.find(value_string);
  if (iter == pending_distance_items_.end())
    return false;

  PendingDistanceItem dist_item = iter->second;
  pending_distance_items_.erase(iter);

  size_t distance = data_->size() - dist_item.pos;
  switch (dist_item.data_size) {
    case 4:
      return ConvertAndFillData<uint32_t>(dist_item.pos, distance);
    case 8:
      return ConvertAndFillData<uint64_t>(dist_item.pos, distance);
    default:
      assert(false);
      return false;
  }
}

bool ValidationTestInputParser::ParseHandles(const DataType& type,
                                             std::string_view value_string) {
  // It should be the first item.
  if (!data_->empty())
    return false;

  uint64_t value;
  if (!ConvertToUnsignedInteger(value_string, &value)) {
    return false;
  }

  if (value > std::numeric_limits<size_t>::max())
    return false;

  *num_handles_ = static_cast<size_t>(value);
  return true;
}

bool ValidationTestInputParser::ConvertToUnsignedInteger(
    std::string_view value_string,
    uint64_t* value) {
  return value_string.find_first_of("xX") != std::string_view::npos
             ? base::HexStringToUInt64(value_string, value)
             : base::StringToUint64(value_string, value);
}

bool ValidationTestInputParser::ConvertToSignedInteger(
    std::string_view value_string,
    int64_t* value) {
  return value_string.find_first_of("xX") != std::string_view::npos
             ? base::HexStringToInt64(value_string, value)
             : base::StringToInt64(value_string, value);
}

}  // namespace

bool ParseValidationTestInput(const std::string& input,
                              std::vector<uint8_t>* data,
                              size_t* num_handles,
                              std::string* error_message) {
  ValidationTestInputParser parser(input, data, num_handles, error_message);
  return parser.Run();
}

}  // namespace test
}  // namespace mojo
