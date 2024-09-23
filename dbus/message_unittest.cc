// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "dbus/message.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "dbus/object_path.h"
#include "dbus/test_proto.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

// Test that a byte can be properly written and read. We only have this
// test for byte, as repeating this for other basic types is too redundant.
TEST(MessageTest, AppendAndPopByte) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  writer.AppendByte(123);  // The input is 123.

  MessageReader reader(message.get());
  ASSERT_TRUE(reader.HasMoreData());  // Should have data to read.
  ASSERT_EQ(Message::BYTE, reader.GetDataType());
  ASSERT_EQ("y", reader.GetDataSignature());

  bool bool_value = false;
  // Should fail as the type is not bool here.
  ASSERT_FALSE(reader.PopBool(&bool_value));

  uint8_t byte_value = 0;
  ASSERT_TRUE(reader.PopByte(&byte_value));
  EXPECT_EQ(123, byte_value);          // Should match with the input.
  ASSERT_FALSE(reader.HasMoreData());  // Should not have more data to read.

  // Try to get another byte. Should fail.
  ASSERT_FALSE(reader.PopByte(&byte_value));
}

// Check all basic types can be properly written and read.
TEST(MessageTest, AppendAndPopBasicDataTypes) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  // Append 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path".
  writer.AppendByte(0);
  writer.AppendBool(true);
  writer.AppendInt16(2);
  writer.AppendUint16(3);
  writer.AppendInt32(4);
  writer.AppendUint32(5);
  writer.AppendInt64(6);
  writer.AppendUint64(7);
  writer.AppendDouble(8.0);
  writer.AppendString("string");
  writer.AppendObjectPath(ObjectPath("/object/path"));

  uint8_t byte_value = 0;
  bool bool_value = false;
  int16_t int16_value = 0;
  uint16_t uint16_value = 0;
  int32_t int32_value = 0;
  uint32_t uint32_value = 0;
  int64_t int64_value = 0;
  uint64_t uint64_value = 0;
  double double_value = 0;
  std::string string_value;
  ObjectPath object_path_value;

  MessageReader reader(message.get());
  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_EQ("y", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopByte(&byte_value));
  ASSERT_EQ("b", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopBool(&bool_value));
  ASSERT_EQ("n", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopInt16(&int16_value));
  ASSERT_EQ("q", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopUint16(&uint16_value));
  ASSERT_EQ("i", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopInt32(&int32_value));
  ASSERT_EQ("u", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopUint32(&uint32_value));
  ASSERT_EQ("x", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopInt64(&int64_value));
  ASSERT_EQ("t", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopUint64(&uint64_value));
  ASSERT_EQ("d", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopDouble(&double_value));
  ASSERT_EQ("s", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopString(&string_value));
  ASSERT_EQ("o", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopObjectPath(&object_path_value));
  ASSERT_EQ("", reader.GetDataSignature());
  ASSERT_FALSE(reader.HasMoreData());

  // 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path" should be returned.
  EXPECT_EQ(0, byte_value);
  EXPECT_EQ(true, bool_value);
  EXPECT_EQ(2, int16_value);
  EXPECT_EQ(3U, uint16_value);
  EXPECT_EQ(4, int32_value);
  EXPECT_EQ(5U, uint32_value);
  EXPECT_EQ(6, int64_value);
  EXPECT_EQ(7U, uint64_value);
  EXPECT_DOUBLE_EQ(8.0, double_value);
  EXPECT_EQ("string", string_value);
  EXPECT_EQ(ObjectPath("/object/path"), object_path_value);
}

// Check all basic types can be properly written and read.
TEST(MessageTest, AppendAndPopFileDescriptor) {
  if (!IsDBusTypeUnixFdSupported()) {
    LOG(WARNING) << "FD passing is not supported";
    return;
  }

  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  // Append stdout.
  const int fd_in = 1;
  writer.AppendFileDescriptor(fd_in);

  base::ScopedFD fd_out;

  MessageReader reader(message.get());
  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_EQ(Message::UNIX_FD, reader.GetDataType());
  ASSERT_EQ("h", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopFileDescriptor(&fd_out));
  ASSERT_FALSE(reader.HasMoreData());

  // Stdout should be returned but we cannot check the descriptor
  // value because stdout will be dup'd.  Instead check st_rdev
  // which should be identical.
  struct stat sb_stdout;
  int status_stdout = HANDLE_EINTR(fstat(fd_in, &sb_stdout));
  ASSERT_GE(status_stdout, 0);
  struct stat sb_fd;
  int status_fd = HANDLE_EINTR(fstat(fd_out.get(), &sb_fd));
  ASSERT_GE(status_fd, 0);
  EXPECT_EQ(sb_stdout.st_rdev, sb_fd.st_rdev);
}

// Check all variant types can be properly written and read.
TEST(MessageTest, AppendAndPopVariantDataTypes) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());

  // Append 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path".
  writer.AppendVariantOfByte(0);
  writer.AppendVariantOfBool(true);
  writer.AppendVariantOfInt16(2);
  writer.AppendVariantOfUint16(3);
  writer.AppendVariantOfInt32(4);
  writer.AppendVariantOfUint32(5);
  writer.AppendVariantOfInt64(6);
  writer.AppendVariantOfUint64(7);
  writer.AppendVariantOfDouble(8.0);
  writer.AppendVariantOfString("string");
  writer.AppendVariantOfObjectPath(ObjectPath("/object/path"));

  uint8_t byte_value = 0;
  bool bool_value = false;
  int16_t int16_value = 0;
  uint16_t uint16_value = 0;
  int32_t int32_value = 0;
  uint32_t uint32_value = 0;
  int64_t int64_value = 0;
  uint64_t uint64_value = 0;
  double double_value = 0;
  std::string string_value;
  ObjectPath object_path_value;

  MessageReader reader(message.get());
  ASSERT_TRUE(reader.HasMoreData());
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfByte(&byte_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfBool(&bool_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfInt16(&int16_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfUint16(&uint16_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfInt32(&int32_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfUint32(&uint32_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfInt64(&int64_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfUint64(&uint64_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfDouble(&double_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfString(&string_value));
  ASSERT_EQ("v", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopVariantOfObjectPath(&object_path_value));
  ASSERT_EQ("", reader.GetDataSignature());
  ASSERT_FALSE(reader.HasMoreData());

  // 0, 1, 2, 3, 4, 5, 6, 7, 8, "string", "/object/path" should be returned.
  EXPECT_EQ(0, byte_value);
  EXPECT_EQ(true, bool_value);
  EXPECT_EQ(2, int16_value);
  EXPECT_EQ(3U, uint16_value);
  EXPECT_EQ(4, int32_value);
  EXPECT_EQ(5U, uint32_value);
  EXPECT_EQ(6, int64_value);
  EXPECT_EQ(7U, uint64_value);
  EXPECT_DOUBLE_EQ(8.0, double_value);
  EXPECT_EQ("string", string_value);
  EXPECT_EQ(ObjectPath("/object/path"), object_path_value);
}

TEST(MessageTest, ArrayOfBytes) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<uint8_t> bytes;
  bytes.push_back(1);
  bytes.push_back(2);
  bytes.push_back(3);
  writer.AppendArrayOfBytes(bytes);

  MessageReader reader(message.get());
  const uint8_t* output_bytes = nullptr;
  size_t length = 0;
  ASSERT_EQ("ay", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfBytes(&output_bytes, &length));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(3U, length);
  EXPECT_EQ(1, output_bytes[0]);
  EXPECT_EQ(2, output_bytes[1]);
  EXPECT_EQ(3, output_bytes[2]);
}

TEST(MessageTest, ArrayOfInt32s) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<int32_t> int32s;
  int32s.push_back(1);
  int32s.push_back(2);
  int32s.push_back(3);
  writer.AppendArrayOfInt32s(int32s);

  MessageReader reader(message.get());
  const int32_t* output_int32s = nullptr;
  size_t length = 0;
  ASSERT_EQ("ai", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfInt32s(&output_int32s, &length));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(3U, length);
  EXPECT_EQ(1, output_int32s[0]);
  EXPECT_EQ(2, output_int32s[1]);
  EXPECT_EQ(3, output_int32s[2]);
}

TEST(MessageTest, ArrayOfUint32s) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<uint32_t> uint32s;
  uint32s.push_back(1);
  uint32s.push_back(2);
  uint32s.push_back(3);
  writer.AppendArrayOfUint32s(uint32s);

  MessageReader reader(message.get());
  const uint32_t* output_uint32s = nullptr;
  size_t length = 0;
  ASSERT_EQ("au", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfUint32s(&output_uint32s, &length));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(3U, length);
  EXPECT_EQ(1U, output_uint32s[0]);
  EXPECT_EQ(2U, output_uint32s[1]);
  EXPECT_EQ(3U, output_uint32s[2]);
}

TEST(MessageTest, ArrayOfDoubles) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<double> doubles;
  doubles.push_back(0.2);
  doubles.push_back(0.5);
  doubles.push_back(1);
  writer.AppendArrayOfDoubles(doubles);

  MessageReader reader(message.get());
  const double* output_doubles = nullptr;
  size_t length = 0;
  ASSERT_EQ("ad", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfDoubles(&output_doubles, &length));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(3U, length);
  EXPECT_EQ(0.2, output_doubles[0]);
  EXPECT_EQ(0.5, output_doubles[1]);
  EXPECT_EQ(1, output_doubles[2]);
}

TEST(MessageTest, ArrayOfBytes_Empty) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<uint8_t> bytes;
  writer.AppendArrayOfBytes(bytes);

  MessageReader reader(message.get());
  const uint8_t* output_bytes = nullptr;
  size_t length = 0;
  ASSERT_EQ("ay", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfBytes(&output_bytes, &length));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(0U, length);
  EXPECT_EQ(nullptr, output_bytes);
}

TEST(MessageTest, ArrayOfStrings) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<std::string> strings;
  strings.push_back("fee");
  strings.push_back("fie");
  strings.push_back("foe");
  strings.push_back("fum");
  writer.AppendArrayOfStrings(strings);

  MessageReader reader(message.get());
  std::vector<std::string> output_strings;
  ASSERT_EQ("as", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfStrings(&output_strings));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(4U, output_strings.size());
  EXPECT_EQ("fee", output_strings[0]);
  EXPECT_EQ("fie", output_strings[1]);
  EXPECT_EQ("foe", output_strings[2]);
  EXPECT_EQ("fum", output_strings[3]);
}

TEST(MessageTest, ArrayOfObjectPaths) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  std::vector<ObjectPath> object_paths;
  object_paths.push_back(ObjectPath("/object/path/1"));
  object_paths.push_back(ObjectPath("/object/path/2"));
  object_paths.push_back(ObjectPath("/object/path/3"));
  writer.AppendArrayOfObjectPaths(object_paths);

  MessageReader reader(message.get());
  std::vector<ObjectPath> output_object_paths;
  ASSERT_EQ("ao", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfObjectPaths(&output_object_paths));
  ASSERT_FALSE(reader.HasMoreData());
  ASSERT_EQ(3U, output_object_paths.size());
  EXPECT_EQ(ObjectPath("/object/path/1"), output_object_paths[0]);
  EXPECT_EQ(ObjectPath("/object/path/2"), output_object_paths[1]);
  EXPECT_EQ(ObjectPath("/object/path/3"), output_object_paths[2]);
}

TEST(MessageTest, ProtoBuf) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  TestProto send_message;
  send_message.set_text("testing");
  send_message.set_number(123);
  writer.AppendProtoAsArrayOfBytes(send_message);

  MessageReader reader(message.get());
  TestProto receive_message;
  ASSERT_EQ("ay", reader.GetDataSignature());
  ASSERT_TRUE(reader.PopArrayOfBytesAsProto(&receive_message));
  EXPECT_EQ(receive_message.text(), send_message.text());
  EXPECT_EQ(receive_message.number(), send_message.number());
}

// Test that an array can be properly written and read. We only have this
// test for array, as repeating this for other container types is too
// redundant.
TEST(MessageTest, OpenArrayAndPopArray) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  MessageWriter array_writer(nullptr);
  writer.OpenArray("s", &array_writer);  // Open an array of strings.
  array_writer.AppendString("foo");
  array_writer.AppendString("bar");
  array_writer.AppendString("baz");
  writer.CloseContainer(&array_writer);

  MessageReader reader(message.get());
  ASSERT_EQ(Message::ARRAY, reader.GetDataType());
  ASSERT_EQ("as", reader.GetDataSignature());
  MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));
  ASSERT_FALSE(reader.HasMoreData());  // Should not have more data to read.

  std::string string_value;
  ASSERT_TRUE(array_reader.PopString(&string_value));
  EXPECT_EQ("foo", string_value);
  ASSERT_TRUE(array_reader.PopString(&string_value));
  EXPECT_EQ("bar", string_value);
  ASSERT_TRUE(array_reader.PopString(&string_value));
  EXPECT_EQ("baz", string_value);
  // Should not have more data to read.
  ASSERT_FALSE(array_reader.HasMoreData());
}

// Create a complex message using array, struct, variant, dict entry, and
// make sure it can be read properly.
TEST(MessageTest, CreateComplexMessageAndReadIt) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  {
    MessageWriter array_writer(nullptr);
    // Open an array of variants.
    writer.OpenArray("v", &array_writer);
    {
      // The first value in the array.
      {
        MessageWriter variant_writer(nullptr);
        // Open a variant of a boolean.
        array_writer.OpenVariant("b", &variant_writer);
        variant_writer.AppendBool(true);
        array_writer.CloseContainer(&variant_writer);
      }

      // The second value in the array.
      {
        MessageWriter variant_writer(nullptr);
        // Open a variant of a struct that contains a string and an int32_t.
        array_writer.OpenVariant("(si)", &variant_writer);
        {
          MessageWriter struct_writer(nullptr);
          variant_writer.OpenStruct(&struct_writer);
          struct_writer.AppendString("string");
          struct_writer.AppendInt32(123);
          variant_writer.CloseContainer(&struct_writer);
        }
        array_writer.CloseContainer(&variant_writer);
      }

      // The third value in the array.
      {
        MessageWriter variant_writer(nullptr);
        // Open a variant of an array of string-to-int64_t dict entries.
        array_writer.OpenVariant("a{sx}", &variant_writer);
        {
          // Opens an array of string-to-int64_t dict entries.
          MessageWriter dict_array_writer(nullptr);
          variant_writer.OpenArray("{sx}", &dict_array_writer);
          {
            // Opens a string-to-int64_t dict entries.
            MessageWriter dict_entry_writer(nullptr);
            dict_array_writer.OpenDictEntry(&dict_entry_writer);
            dict_entry_writer.AppendString("foo");
            dict_entry_writer.AppendInt64(INT64_C(1234567890123456789));
            dict_array_writer.CloseContainer(&dict_entry_writer);
          }
          variant_writer.CloseContainer(&dict_array_writer);
        }
        array_writer.CloseContainer(&variant_writer);
      }
    }
    writer.CloseContainer(&array_writer);
  }
  // What we have created looks like this:
  EXPECT_EQ(
      "message_type: MESSAGE_METHOD_RETURN\n"
      "signature: av\n"
      "\n"
      "array [\n"
      "  variant     bool true\n"
      "  variant     struct {\n"
      "      string \"string\"\n"
      "      int32_t 123\n"
      "    }\n"
      "  variant     array [\n"
      "      dict entry {\n"
      "        string \"foo\"\n"
      "        int64_t 1234567890123456789\n"
      "      }\n"
      "    ]\n"
      "]\n",
      message->ToString());

  MessageReader reader(message.get());
  ASSERT_EQ("av", reader.GetDataSignature());
  MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));

  // The first value in the array.
  bool bool_value = false;
  ASSERT_EQ("v", array_reader.GetDataSignature());
  ASSERT_TRUE(array_reader.PopVariantOfBool(&bool_value));
  EXPECT_EQ(true, bool_value);

  // The second value in the array.
  {
    MessageReader variant_reader(nullptr);
    ASSERT_TRUE(array_reader.PopVariant(&variant_reader));
    {
      MessageReader struct_reader(nullptr);
      ASSERT_EQ("(si)", variant_reader.GetDataSignature());
      ASSERT_TRUE(variant_reader.PopStruct(&struct_reader));
      std::string string_value;
      ASSERT_TRUE(struct_reader.PopString(&string_value));
      EXPECT_EQ("string", string_value);
      int32_t int32_value = 0;
      ASSERT_TRUE(struct_reader.PopInt32(&int32_value));
      EXPECT_EQ(123, int32_value);
      ASSERT_FALSE(struct_reader.HasMoreData());
    }
    ASSERT_FALSE(variant_reader.HasMoreData());
  }

  // The third value in the array.
  {
    MessageReader variant_reader(nullptr);
    ASSERT_TRUE(array_reader.PopVariant(&variant_reader));
    {
      MessageReader dict_array_reader(nullptr);
      ASSERT_EQ("a{sx}", variant_reader.GetDataSignature());
      ASSERT_TRUE(variant_reader.PopArray(&dict_array_reader));
      {
        MessageReader dict_entry_reader(nullptr);
        ASSERT_TRUE(dict_array_reader.PopDictEntry(&dict_entry_reader));
        std::string string_value;
        ASSERT_TRUE(dict_entry_reader.PopString(&string_value));
        EXPECT_EQ("foo", string_value);
        int64_t int64_value = 0;
        ASSERT_TRUE(dict_entry_reader.PopInt64(&int64_value));
        EXPECT_EQ(INT64_C(1234567890123456789), int64_value);
      }
      ASSERT_FALSE(dict_array_reader.HasMoreData());
    }
    ASSERT_FALSE(variant_reader.HasMoreData());
  }
  ASSERT_FALSE(array_reader.HasMoreData());
  ASSERT_FALSE(reader.HasMoreData());
}

TEST(MessageTest, MethodCall) {
  MethodCall method_call("com.example.Interface", "SomeMethod");
  EXPECT_NE(nullptr, method_call.raw_message());
  EXPECT_EQ(Message::MESSAGE_METHOD_CALL, method_call.GetMessageType());
  EXPECT_EQ("MESSAGE_METHOD_CALL", method_call.GetMessageTypeAsString());
  method_call.SetDestination("com.example.Service");
  method_call.SetPath(ObjectPath("/com/example/Object"));

  MessageWriter writer(&method_call);
  writer.AppendString("payload");

  EXPECT_EQ(
      "message_type: MESSAGE_METHOD_CALL\n"
      "destination: com.example.Service\n"
      "path: /com/example/Object\n"
      "interface: com.example.Interface\n"
      "member: SomeMethod\n"
      "signature: s\n"
      "\n"
      "string \"payload\"\n",
      method_call.ToString());
}

TEST(MessageTest, MethodCall_FromRawMessage) {
  DBusMessage* raw_message = dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL);
  dbus_message_set_interface(raw_message, "com.example.Interface");
  dbus_message_set_member(raw_message, "SomeMethod");

  std::unique_ptr<MethodCall> method_call(
      MethodCall::FromRawMessage(raw_message));
  EXPECT_EQ("com.example.Interface", method_call->GetInterface());
  EXPECT_EQ("SomeMethod", method_call->GetMember());
}

TEST(MessageTest, Signal) {
  Signal signal("com.example.Interface", "SomeSignal");
  EXPECT_NE(nullptr, signal.raw_message());
  EXPECT_EQ(Message::MESSAGE_SIGNAL, signal.GetMessageType());
  EXPECT_EQ("MESSAGE_SIGNAL", signal.GetMessageTypeAsString());
  signal.SetPath(ObjectPath("/com/example/Object"));

  MessageWriter writer(&signal);
  writer.AppendString("payload");

  EXPECT_EQ(
      "message_type: MESSAGE_SIGNAL\n"
      "path: /com/example/Object\n"
      "interface: com.example.Interface\n"
      "member: SomeSignal\n"
      "signature: s\n"
      "\n"
      "string \"payload\"\n",
      signal.ToString());
}

TEST(MessageTest, Signal_FromRawMessage) {
  DBusMessage* raw_message = dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL);
  dbus_message_set_interface(raw_message, "com.example.Interface");
  dbus_message_set_member(raw_message, "SomeSignal");

  std::unique_ptr<Signal> signal(Signal::FromRawMessage(raw_message));
  EXPECT_EQ("com.example.Interface", signal->GetInterface());
  EXPECT_EQ("SomeSignal", signal->GetMember());
}

TEST(MessageTest, Response) {
  std::unique_ptr<Response> response(Response::CreateEmpty());
  EXPECT_TRUE(response->raw_message());
  EXPECT_EQ(Message::MESSAGE_METHOD_RETURN, response->GetMessageType());
  EXPECT_EQ("MESSAGE_METHOD_RETURN", response->GetMessageTypeAsString());
}

TEST(MessageTest, Response_FromMethodCall) {
  const uint32_t kSerial = 123;
  MethodCall method_call("com.example.Interface", "SomeMethod");
  method_call.SetSerial(kSerial);

  std::unique_ptr<Response> response(Response::FromMethodCall(&method_call));
  EXPECT_EQ(Message::MESSAGE_METHOD_RETURN, response->GetMessageType());
  EXPECT_EQ("MESSAGE_METHOD_RETURN", response->GetMessageTypeAsString());
  // The serial should be copied to the reply serial.
  EXPECT_EQ(kSerial, response->GetReplySerial());
}

TEST(MessageTest, ErrorResponse_FromMethodCall) {
  const uint32_t kSerial = 123;
  const char kErrorMessage[] = "error message";

  MethodCall method_call("com.example.Interface", "SomeMethod");
  method_call.SetSerial(kSerial);

  std::unique_ptr<ErrorResponse> error_response(ErrorResponse::FromMethodCall(
      &method_call, DBUS_ERROR_FAILED, kErrorMessage));
  EXPECT_EQ(Message::MESSAGE_ERROR, error_response->GetMessageType());
  EXPECT_EQ("MESSAGE_ERROR", error_response->GetMessageTypeAsString());
  // The serial should be copied to the reply serial.
  EXPECT_EQ(kSerial, error_response->GetReplySerial());

  // Error message should be added to the payload.
  MessageReader reader(error_response.get());
  std::string error_message;
  ASSERT_TRUE(reader.PopString(&error_message));
  EXPECT_EQ(kErrorMessage, error_message);
}

TEST(MessageTest, GetAndSetHeaders) {
  std::unique_ptr<Response> message(Response::CreateEmpty());

  EXPECT_EQ("", message->GetDestination());
  EXPECT_EQ(ObjectPath(std::string()), message->GetPath());
  EXPECT_EQ("", message->GetInterface());
  EXPECT_EQ("", message->GetMember());
  EXPECT_EQ("", message->GetErrorName());
  EXPECT_EQ("", message->GetSender());
  EXPECT_EQ(0U, message->GetSerial());
  EXPECT_EQ(0U, message->GetReplySerial());

  EXPECT_TRUE(message->SetDestination("org.chromium.destination"));
  EXPECT_TRUE(message->SetPath(ObjectPath("/org/chromium/path")));
  EXPECT_TRUE(message->SetInterface("org.chromium.interface"));
  EXPECT_TRUE(message->SetMember("member"));
  EXPECT_TRUE(message->SetErrorName("org.chromium.error"));
  EXPECT_TRUE(message->SetSender(":1.2"));
  message->SetSerial(123);
  message->SetReplySerial(456);

  EXPECT_EQ("org.chromium.destination", message->GetDestination());
  EXPECT_EQ(ObjectPath("/org/chromium/path"), message->GetPath());
  EXPECT_EQ("org.chromium.interface", message->GetInterface());
  EXPECT_EQ("member", message->GetMember());
  EXPECT_EQ("org.chromium.error", message->GetErrorName());
  EXPECT_EQ(":1.2", message->GetSender());
  EXPECT_EQ(123U, message->GetSerial());
  EXPECT_EQ(456U, message->GetReplySerial());
}

TEST(MessageTest, SetInvalidHeaders) {
  std::unique_ptr<Response> message(Response::CreateEmpty());
  EXPECT_EQ("", message->GetDestination());
  EXPECT_EQ(ObjectPath(std::string()), message->GetPath());
  EXPECT_EQ("", message->GetInterface());
  EXPECT_EQ("", message->GetMember());
  EXPECT_EQ("", message->GetErrorName());
  EXPECT_EQ("", message->GetSender());

  // Empty element between periods.
  EXPECT_FALSE(message->SetDestination("org..chromium"));
  // Trailing '/' is only allowed for the root path.
  EXPECT_FALSE(message->SetPath(ObjectPath("/org/chromium/")));
  // Interface name cannot contain '/'.
  EXPECT_FALSE(message->SetInterface("org/chromium/interface"));
  // Member name cannot begin with a digit.
  EXPECT_FALSE(message->SetMember("1member"));
  // Error name cannot begin with a period.
  EXPECT_FALSE(message->SetErrorName(".org.chromium.error"));
  // Disallowed characters.
  EXPECT_FALSE(message->SetSender("?!#*"));

  EXPECT_EQ("", message->GetDestination());
  EXPECT_EQ(ObjectPath(std::string()), message->GetPath());
  EXPECT_EQ("", message->GetInterface());
  EXPECT_EQ("", message->GetMember());
  EXPECT_EQ("", message->GetErrorName());
  EXPECT_EQ("", message->GetSender());
}

TEST(MessageTest, ToString_LongString) {
  const std::string kLongString(1000, 'o');

  std::unique_ptr<Response> message(Response::CreateEmpty());
  MessageWriter writer(message.get());
  writer.AppendString(kLongString);

  ASSERT_EQ(
      "message_type: MESSAGE_METHOD_RETURN\n"
      "signature: s\n\n"
      "string \"oooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooo... "
      "(1000 bytes in total)\"\n",
      message->ToString());
}

}  // namespace dbus
