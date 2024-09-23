// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/values_util.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "dbus/message.h"

namespace dbus {

namespace {

// Returns whether |value| is exactly representable by double or not.
template <typename T>
bool IsExactlyRepresentableByDouble(T value) {
  return value == static_cast<T>(static_cast<double>(value));
}

// Pops values from |reader| and appends them to |list_value|.
bool PopListElements(MessageReader* reader, base::Value::List& list_value) {
  while (reader->HasMoreData()) {
    base::Value element_value = PopDataAsValue(reader);
    if (element_value.is_none())
      return false;
    list_value.Append(std::move(element_value));
  }
  return true;
}

// Pops dict-entries from |reader| and sets them to |dictionary_value|
bool PopDictionaryEntries(MessageReader* reader,
                          base::Value::Dict& dictionary_value) {
  while (reader->HasMoreData()) {
    DCHECK_EQ(Message::DICT_ENTRY, reader->GetDataType());
    MessageReader entry_reader(nullptr);
    if (!reader->PopDictEntry(&entry_reader))
      return false;
    // Get key as a string.
    std::string key_string;
    if (entry_reader.GetDataType() == Message::STRING) {
      // If the type of keys is STRING, pop it directly.
      if (!entry_reader.PopString(&key_string))
        return false;
    } else {
      // If the type of keys is not STRING, convert it to string.
      base::Value key = PopDataAsValue(&entry_reader);
      if (key.is_none())
        return false;
      // Use JSONWriter to convert an arbitrary value to a string.
      base::JSONWriter::Write(key, &key_string);
    }
    // Get the value and set the key-value pair.
    base::Value value = PopDataAsValue(&entry_reader);
    if (value.is_none())
      return false;
    dictionary_value.Set(key_string, std::move(value));
  }
  return true;
}

// Gets the D-Bus type signature for the value.
std::string GetTypeSignature(base::ValueView value) {
  struct Visitor {
    std::string operator()(absl::monostate) {
      DLOG(ERROR) << "Unexpected type " << base::Value::Type::NONE;
      return std::string();
    }

    std::string operator()(bool) { return "b"; }

    std::string operator()(int) { return "i"; }

    std::string operator()(double) { return "d"; }

    std::string operator()(std::string_view) { return "s"; }

    std::string operator()(const base::Value::BlobStorage&) { return "ay"; }

    std::string operator()(const base::Value::Dict&) { return "a{sv}"; }

    std::string operator()(const base::Value::List&) { return "av"; }
  };
  return value.Visit(Visitor());
}

}  // namespace

base::Value PopDataAsValue(MessageReader* reader) {
  base::Value result;
  switch (reader->GetDataType()) {
    case Message::INVALID_DATA:
      // Do nothing.
      break;
    case Message::BYTE: {
      uint8_t value = 0;
      if (reader->PopByte(&value))
        result = base::Value(value);
      break;
    }
    case Message::BOOL: {
      bool value = false;
      if (reader->PopBool(&value))
        result = base::Value(value);
      break;
    }
    case Message::INT16: {
      int16_t value = 0;
      if (reader->PopInt16(&value))
        result = base::Value(value);
      break;
    }
    case Message::UINT16: {
      uint16_t value = 0;
      if (reader->PopUint16(&value))
        result = base::Value(value);
      break;
    }
    case Message::INT32: {
      int32_t value = 0;
      if (reader->PopInt32(&value))
        result = base::Value(value);
      break;
    }
    case Message::UINT32: {
      uint32_t value = 0;
      if (reader->PopUint32(&value)) {
        result = base::Value(static_cast<double>(value));
      }
      break;
    }
    case Message::INT64: {
      int64_t value = 0;
      if (reader->PopInt64(&value)) {
        DLOG_IF(WARNING, !IsExactlyRepresentableByDouble(value))
            << value << " is not exactly representable by double";
        result = base::Value(static_cast<double>(value));
      }
      break;
    }
    case Message::UINT64: {
      uint64_t value = 0;
      if (reader->PopUint64(&value)) {
        DLOG_IF(WARNING, !IsExactlyRepresentableByDouble(value))
            << value << " is not exactly representable by double";
        result = base::Value(static_cast<double>(value));
      }
      break;
    }
    case Message::DOUBLE: {
      double value = 0;
      if (reader->PopDouble(&value))
        result = base::Value(value);
      break;
    }
    case Message::STRING: {
      std::string value;
      if (reader->PopString(&value))
        result = base::Value(value);
      break;
    }
    case Message::OBJECT_PATH: {
      ObjectPath value;
      if (reader->PopObjectPath(&value))
        result = base::Value(value.value());
      break;
    }
    case Message::UNIX_FD: {
      // Cannot distinguish a file descriptor from an int
      NOTREACHED();
    }
    case Message::ARRAY: {
      MessageReader sub_reader(nullptr);
      if (reader->PopArray(&sub_reader)) {
        // If the type of the array's element is DICT_ENTRY, create a
        // Value with type base::Value::Dict, otherwise create a
        // Value with type base::Value::List.
        if (sub_reader.GetDataType() == Message::DICT_ENTRY) {
          base::Value::Dict dictionary_value;
          if (PopDictionaryEntries(&sub_reader, dictionary_value))
            result = base::Value(std::move(dictionary_value));
        } else {
          base::Value::List list_value;
          if (PopListElements(&sub_reader, list_value))
            result = base::Value(std::move(list_value));
        }
      }
      break;
    }
    case Message::STRUCT: {
      MessageReader sub_reader(nullptr);
      if (reader->PopStruct(&sub_reader)) {
        base::Value::List list_value;
        if (PopListElements(&sub_reader, list_value))
          result = base::Value(std::move(list_value));
      }
      break;
    }
    case Message::DICT_ENTRY:
      // DICT_ENTRY must be popped as an element of an array.
      NOTREACHED();
    case Message::VARIANT: {
      MessageReader sub_reader(nullptr);
      if (reader->PopVariant(&sub_reader))
        result = PopDataAsValue(&sub_reader);
      break;
    }
  }
  return result;
}

void AppendBasicTypeValueData(MessageWriter* writer, base::ValueView value) {
  struct Visitor {
    raw_ptr<MessageWriter> writer;

    void operator()(absl::monostate) {
      DLOG(ERROR) << "Unexpected type: " << base::Value::Type::NONE;
    }

    void operator()(bool value) { writer->AppendBool(value); }

    void operator()(int value) { writer->AppendInt32(value); }

    void operator()(double value) { writer->AppendDouble(value); }

    void operator()(std::string_view value) {
      writer->AppendString(std::string(value));
    }

    void operator()(const base::Value::BlobStorage&) {
      DLOG(ERROR) << "Unexpected type: " << base::Value::Type::BINARY;
    }

    void operator()(const base::Value::Dict&) {
      DLOG(ERROR) << "Unexpected type: " << base::Value::Type::DICT;
    }

    void operator()(const base::Value::List&) {
      DLOG(ERROR) << "Unexpected type: " << base::Value::Type::LIST;
    }
  };

  value.Visit(Visitor{.writer = writer});
}

void AppendBasicTypeValueDataAsVariant(MessageWriter* writer,
                                       base::ValueView value) {
  MessageWriter sub_writer(nullptr);
  writer->OpenVariant(GetTypeSignature(value), &sub_writer);
  AppendBasicTypeValueData(&sub_writer, value);
  writer->CloseContainer(&sub_writer);
}

void AppendValueData(MessageWriter* writer, base::ValueView value) {
  struct Visitor {
    raw_ptr<MessageWriter> writer;

    void operator()(absl::monostate) {
      DLOG(ERROR) << "Unexpected type: " << base::Value::Type::NONE;
    }

    void operator()(bool value) {
      return AppendBasicTypeValueData(writer, value);
    }

    void operator()(int value) {
      return AppendBasicTypeValueData(writer, value);
    }

    void operator()(double value) {
      return AppendBasicTypeValueData(writer, value);
    }

    void operator()(std::string_view value) {
      return AppendBasicTypeValueData(writer, value);
    }

    void operator()(const base::Value::BlobStorage& value) {
      DLOG(ERROR) << "Unexpected type: " << base::Value::Type::BINARY;
    }

    void operator()(const base::Value::Dict& value) {
      dbus::MessageWriter array_writer(nullptr);
      writer->OpenArray("{sv}", &array_writer);
      for (auto item : value) {
        dbus::MessageWriter dict_entry_writer(nullptr);
        array_writer.OpenDictEntry(&dict_entry_writer);
        dict_entry_writer.AppendString(item.first);
        AppendValueDataAsVariant(&dict_entry_writer, item.second);
        array_writer.CloseContainer(&dict_entry_writer);
      }
      writer->CloseContainer(&array_writer);
    }

    void operator()(const base::Value::List& value) {
      dbus::MessageWriter array_writer(nullptr);
      writer->OpenArray("v", &array_writer);
      for (const auto& value_in_list : value) {
        AppendValueDataAsVariant(&array_writer, value_in_list);
      }
      writer->CloseContainer(&array_writer);
    }
  };

  value.Visit(Visitor{.writer = writer});
}

void AppendValueDataAsVariant(MessageWriter* writer, base::ValueView value) {
  MessageWriter variant_writer(nullptr);
  writer->OpenVariant(GetTypeSignature(value), &variant_writer);
  AppendValueData(&variant_writer, value);
  writer->CloseContainer(&variant_writer);
}

}  // namespace dbus
