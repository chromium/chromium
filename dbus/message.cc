// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/message.h"

#include <string>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "dbus/object_path.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace {

// Appends the header name and the value to |output|, if the value is
// not empty.
void AppendStringHeader(const std::string& header_name,
                        const std::string& header_value,
                        std::string* output) {
  if (!header_value.empty()) {
    *output += header_name + ": " + header_value + "\n";
  }
}

// Appends the header name and the value to |output|, if the value is
// nonzero.
void AppendUint32Header(const std::string& header_name,
                        uint32_t header_value,
                        std::string* output) {
  if (header_value != 0) {
    *output += (header_name + ": " + base::NumberToString(header_value) + "\n");
  }
}

}  // namespace

namespace dbus {

bool IsDBusTypeUnixFdSupported() {
  int major = 0, minor = 0, micro = 0;
  dbus_get_version(&major, &minor, &micro);
  return major >= 1 && minor >= 4;
}

Message::Message() : raw_message_(nullptr) {}

Message::~Message() {
  if (raw_message_)
    dbus_message_unref(raw_message_);
}

void Message::Init(DBusMessage* raw_message) {
  DCHECK(!raw_message_);
  raw_message_ = raw_message;
}

Message::MessageType Message::GetMessageType() {
  if (!raw_message_)
    return MESSAGE_INVALID;
  const int type = dbus_message_get_type(raw_message_);
  return static_cast<Message::MessageType>(type);
}

std::string Message::GetMessageTypeAsString() {
  switch (GetMessageType()) {
    case MESSAGE_INVALID:
      return "MESSAGE_INVALID";
    case MESSAGE_METHOD_CALL:
      return "MESSAGE_METHOD_CALL";
    case MESSAGE_METHOD_RETURN:
      return "MESSAGE_METHOD_RETURN";
    case MESSAGE_SIGNAL:
      return "MESSAGE_SIGNAL";
    case MESSAGE_ERROR:
      return "MESSAGE_ERROR";
  }
  NOTREACHED();
}

std::string Message::ToStringInternal(const std::string& indent,
                                      MessageReader* reader) {
  const char* kBrokenMessage = "[broken message]";
  std::string output;
  while (reader->HasMoreData()) {
    const DataType type = reader->GetDataType();
    switch (type) {
      case BYTE: {
        uint8_t value = 0;
        if (!reader->PopByte(&value))
          return kBrokenMessage;
        output += indent + "byte " + base::NumberToString(value) + "\n";
        break;
      }
      case BOOL: {
        bool value = false;
        if (!reader->PopBool(&value))
          return kBrokenMessage;
        output += indent + "bool " + (value ? "true" : "false") + "\n";
        break;
      }
      case INT16: {
        int16_t value = 0;
        if (!reader->PopInt16(&value))
          return kBrokenMessage;
        output += indent + "int16_t " + base::NumberToString(value) + "\n";
        break;
      }
      case UINT16: {
        uint16_t value = 0;
        if (!reader->PopUint16(&value))
          return kBrokenMessage;
        output += indent + "uint16_t " + base::NumberToString(value) + "\n";
        break;
      }
      case INT32: {
        int32_t value = 0;
        if (!reader->PopInt32(&value))
          return kBrokenMessage;
        output += indent + "int32_t " + base::NumberToString(value) + "\n";
        break;
      }
      case UINT32: {
        uint32_t value = 0;
        if (!reader->PopUint32(&value))
          return kBrokenMessage;
        output += indent + "uint32_t " + base::NumberToString(value) + "\n";
        break;
      }
      case INT64: {
        int64_t value = 0;
        if (!reader->PopInt64(&value))
          return kBrokenMessage;
        output += (indent + "int64_t " + base::NumberToString(value) + "\n");
        break;
      }
      case UINT64: {
        uint64_t value = 0;
        if (!reader->PopUint64(&value))
          return kBrokenMessage;
        output += (indent + "uint64_t " + base::NumberToString(value) + "\n");
        break;
      }
      case DOUBLE: {
        double value = 0;
        if (!reader->PopDouble(&value))
          return kBrokenMessage;
        output += indent + "double " + base::NumberToString(value) + "\n";
        break;
      }
      case STRING: {
        std::string value;
        if (!reader->PopString(&value))
          return kBrokenMessage;
        // Truncate if the string is longer than the limit.
        const size_t kTruncateLength = 100;
        if (value.size() < kTruncateLength) {
          output += indent + "string \"" + value + "\"\n";
        } else {
          std::string truncated;
          base::TruncateUTF8ToByteSize(value, kTruncateLength, &truncated);
          base::StringAppendF(&truncated, "... (%" PRIuS " bytes in total)",
                              value.size());
          output += indent + "string \"" + truncated + "\"\n";
        }
        break;
      }
      case OBJECT_PATH: {
        ObjectPath value;
        if (!reader->PopObjectPath(&value))
          return kBrokenMessage;
        output += indent + "object_path \"" + value.value() + "\"\n";
        break;
      }
      case ARRAY: {
        MessageReader sub_reader(this);
        if (!reader->PopArray(&sub_reader))
          return kBrokenMessage;
        output += indent + "array [\n";
        output += ToStringInternal(indent + "  ", &sub_reader);
        output += indent + "]\n";
        break;
      }
      case STRUCT: {
        MessageReader sub_reader(this);
        if (!reader->PopStruct(&sub_reader))
          return kBrokenMessage;
        output += indent + "struct {\n";
        output += ToStringInternal(indent + "  ", &sub_reader);
        output += indent + "}\n";
        break;
      }
      case DICT_ENTRY: {
        MessageReader sub_reader(this);
        if (!reader->PopDictEntry(&sub_reader))
          return kBrokenMessage;
        output += indent + "dict entry {\n";
        output += ToStringInternal(indent + "  ", &sub_reader);
        output += indent + "}\n";
        break;
      }
      case VARIANT: {
        MessageReader sub_reader(this);
        if (!reader->PopVariant(&sub_reader))
          return kBrokenMessage;
        output += indent + "variant ";
        output += ToStringInternal(indent + "  ", &sub_reader);
        break;
      }
      case UNIX_FD: {
        CHECK(IsDBusTypeUnixFdSupported());

        base::ScopedFD file_descriptor;
        if (!reader->PopFileDescriptor(&file_descriptor))
          return kBrokenMessage;
        output +=
            indent + "fd#" + base::NumberToString(file_descriptor.get()) + "\n";
        break;
      }
      default:
        LOG(FATAL) << "Unknown type: " << type;
    }
  }
  return output;
}

// The returned string consists of message headers such as
// destination if any, followed by a blank line, and the message
// payload. For example, a MethodCall's ToString() will look like:
//
// destination: com.example.Service
// path: /com/example/Object
// interface: com.example.Interface
// member: SomeMethod
//
// string \"payload\"
// ...
std::string Message::ToString() {
  if (!raw_message_)
    return std::string();

  // Generate headers first.
  std::string headers;
  AppendStringHeader("message_type", GetMessageTypeAsString(), &headers);
  AppendStringHeader("destination", GetDestination(), &headers);
  AppendStringHeader("path", GetPath().value(), &headers);
  AppendStringHeader("interface", GetInterface(), &headers);
  AppendStringHeader("member", GetMember(), &headers);
  AppendStringHeader("error_name", GetErrorName(), &headers);
  AppendStringHeader("sender", GetSender(), &headers);
  AppendStringHeader("signature", GetSignature(), &headers);
  AppendUint32Header("serial", GetSerial(), &headers);
  AppendUint32Header("reply_serial", GetReplySerial(), &headers);

  // Generate the payload.
  MessageReader reader(this);
  return headers + "\n" + ToStringInternal(std::string(), &reader);
}

bool Message::SetDestination(const std::string& destination) {
  if (!dbus_validate_bus_name(destination.c_str(), nullptr)) {
    LOG(ERROR) << "Invalid bus name: " << destination;
    return false;
  }
  return dbus_message_set_destination(raw_message_, destination.c_str());
}

bool Message::SetPath(const ObjectPath& path) {
  if (!path.IsValid()) {
    LOG(ERROR) << "Invalid path: " << path.value();
    return false;
  }
  return dbus_message_set_path(raw_message_, path.value().c_str());
}

bool Message::SetInterface(const std::string& interface) {
  if (!dbus_validate_interface(interface.c_str(), nullptr)) {
    LOG(ERROR) << "Invalid interface: " << interface;
    return false;
  }
  return dbus_message_set_interface(raw_message_, interface.c_str());
}

bool Message::SetMember(const std::string& member) {
  if (!dbus_validate_member(member.c_str(), nullptr)) {
    LOG(ERROR) << "Invalid member: " << member;
    return false;
  }
  return dbus_message_set_member(raw_message_, member.c_str());
}

bool Message::SetErrorName(const std::string& error_name) {
  if (!dbus_validate_error_name(error_name.c_str(), nullptr)) {
    LOG(ERROR) << "Invalid error name: " << error_name;
    return false;
  }
  return dbus_message_set_error_name(raw_message_, error_name.c_str());
}

bool Message::SetSender(const std::string& sender) {
  if (!dbus_validate_bus_name(sender.c_str(), nullptr)) {
    LOG(ERROR) << "Invalid bus name: " << sender;
    return false;
  }
  return dbus_message_set_sender(raw_message_, sender.c_str());
}

void Message::SetSerial(uint32_t serial) {
  dbus_message_set_serial(raw_message_, serial);
}

void Message::SetReplySerial(uint32_t reply_serial) {
  dbus_message_set_reply_serial(raw_message_, reply_serial);
}

std::string Message::GetDestination() {
  const char* destination = dbus_message_get_destination(raw_message_);
  return destination ? destination : "";
}

ObjectPath Message::GetPath() {
  const char* path = dbus_message_get_path(raw_message_);
  return ObjectPath(path ? path : "");
}

std::string Message::GetInterface() {
  const char* interface = dbus_message_get_interface(raw_message_);
  return interface ? interface : "";
}

std::string Message::GetMember() {
  const char* member = dbus_message_get_member(raw_message_);
  return member ? member : "";
}

std::string Message::GetErrorName() {
  const char* error_name = dbus_message_get_error_name(raw_message_);
  return error_name ? error_name : "";
}

std::string Message::GetSender() {
  const char* sender = dbus_message_get_sender(raw_message_);
  return sender ? sender : "";
}

std::string Message::GetSignature() {
  const char* signature = dbus_message_get_signature(raw_message_);
  return signature ? signature : "";
}

uint32_t Message::GetSerial() {
  return dbus_message_get_serial(raw_message_);
}

uint32_t Message::GetReplySerial() {
  return dbus_message_get_reply_serial(raw_message_);
}

//
// MethodCall implementation.
//

MethodCall::MethodCall(const std::string& interface_name,
                       const std::string& method_name) {
  Init(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_CALL));

  CHECK(SetInterface(interface_name));
  CHECK(SetMember(method_name));
}

MethodCall::MethodCall() = default;

std::unique_ptr<MethodCall> MethodCall::FromRawMessage(
    DBusMessage* raw_message) {
  DCHECK_EQ(DBUS_MESSAGE_TYPE_METHOD_CALL, dbus_message_get_type(raw_message));

  std::unique_ptr<MethodCall> method_call(new MethodCall());
  method_call->Init(raw_message);
  return method_call;
}

//
// Signal implementation.
//
Signal::Signal(const std::string& interface_name,
               const std::string& method_name) {
  Init(dbus_message_new(DBUS_MESSAGE_TYPE_SIGNAL));

  CHECK(SetInterface(interface_name));
  CHECK(SetMember(method_name));
}

Signal::Signal() = default;

std::unique_ptr<Signal> Signal::FromRawMessage(DBusMessage* raw_message) {
  DCHECK_EQ(DBUS_MESSAGE_TYPE_SIGNAL, dbus_message_get_type(raw_message));

  std::unique_ptr<Signal> signal(new Signal());
  signal->Init(raw_message);
  return signal;
}

//
// Response implementation.
//

Response::Response() = default;

std::unique_ptr<Response> Response::FromRawMessage(DBusMessage* raw_message) {
  DCHECK_EQ(DBUS_MESSAGE_TYPE_METHOD_RETURN,
            dbus_message_get_type(raw_message));

  std::unique_ptr<Response> response(new Response());
  response->Init(raw_message);
  return response;
}

std::unique_ptr<Response> Response::FromMethodCall(MethodCall* method_call) {
  std::unique_ptr<Response> response(new Response());
  response->Init(dbus_message_new_method_return(method_call->raw_message()));
  return response;
}

std::unique_ptr<Response> Response::CreateEmpty() {
  std::unique_ptr<Response> response(new Response());
  response->Init(dbus_message_new(DBUS_MESSAGE_TYPE_METHOD_RETURN));
  return response;
}

//
// ErrorResponse implementation.
//

ErrorResponse::ErrorResponse() = default;

std::unique_ptr<ErrorResponse> ErrorResponse::FromRawMessage(
    DBusMessage* raw_message) {
  DCHECK_EQ(DBUS_MESSAGE_TYPE_ERROR, dbus_message_get_type(raw_message));

  std::unique_ptr<ErrorResponse> response(new ErrorResponse());
  response->Init(raw_message);
  return response;
}

std::unique_ptr<ErrorResponse> ErrorResponse::FromMethodCall(
    MethodCall* method_call,
    const std::string& error_name,
    const std::string& error_message) {
  std::unique_ptr<ErrorResponse> response(new ErrorResponse());
  response->Init(dbus_message_new_error(
      method_call->raw_message(), error_name.c_str(), error_message.c_str()));
  return response;
}

//
// MessageWriter implementation.
//

MessageWriter::MessageWriter(Message* message)
    : message_(message), container_is_open_(false) {
  memset(&raw_message_iter_, 0, sizeof(raw_message_iter_));
  if (message)
    dbus_message_iter_init_append(message_->raw_message(), &raw_message_iter_);
}

MessageWriter::~MessageWriter() = default;

void MessageWriter::AppendByte(uint8_t value) {
  AppendBasic(DBUS_TYPE_BYTE, &value);
}

void MessageWriter::AppendBool(bool value) {
  // The size of dbus_bool_t and the size of bool are different. The
  // former is always 4 per dbus-types.h, whereas the latter is usually 1.
  // dbus_message_iter_append_basic() used in AppendBasic() expects four
  // bytes for DBUS_TYPE_BOOLEAN, so we must pass a dbus_bool_t, instead
  // of a bool, to AppendBasic().
  dbus_bool_t dbus_value = value ? 1 : 0;
  AppendBasic(DBUS_TYPE_BOOLEAN, &dbus_value);
}

void MessageWriter::AppendInt16(int16_t value) {
  AppendBasic(DBUS_TYPE_INT16, &value);
}

void MessageWriter::AppendUint16(uint16_t value) {
  AppendBasic(DBUS_TYPE_UINT16, &value);
}

void MessageWriter::AppendInt32(int32_t value) {
  AppendBasic(DBUS_TYPE_INT32, &value);
}

void MessageWriter::AppendUint32(uint32_t value) {
  AppendBasic(DBUS_TYPE_UINT32, &value);
}

void MessageWriter::AppendInt64(int64_t value) {
  AppendBasic(DBUS_TYPE_INT64, &value);
}

void MessageWriter::AppendUint64(uint64_t value) {
  AppendBasic(DBUS_TYPE_UINT64, &value);
}

void MessageWriter::AppendDouble(double value) {
  AppendBasic(DBUS_TYPE_DOUBLE, &value);
}

void MessageWriter::AppendString(const std::string& value) {
  // D-Bus Specification (0.19) says a string "must be valid UTF-8".
  CHECK(base::IsStringUTF8(value));
  const char* pointer = value.c_str();
  AppendBasic(DBUS_TYPE_STRING, &pointer);
  // TODO(satorux): It may make sense to return an error here, as the
  // input string can be large. If needed, we could add something like
  // bool AppendStringWithErrorChecking().
}

void MessageWriter::AppendObjectPath(const ObjectPath& value) {
  CHECK(value.IsValid());
  const char* pointer = value.value().c_str();
  AppendBasic(DBUS_TYPE_OBJECT_PATH, &pointer);
}

// Ideally, client shouldn't need to supply the signature string, but
// the underlying D-Bus library requires us to supply this before
// appending contents to array and variant. It's technically possible
// for us to design API that doesn't require the signature but it will
// complicate the implementation so we decided to have the signature
// parameter. Hopefully, variants are less used in request messages from
// client side than response message from server side, so this should
// not be a big issue.
void MessageWriter::OpenArray(const std::string& signature,
                              MessageWriter* writer) {
  DCHECK(!container_is_open_);

  const bool success = dbus_message_iter_open_container(
      &raw_message_iter_, DBUS_TYPE_ARRAY, signature.c_str(),
      &writer->raw_message_iter_);
  CHECK(success) << "Unable to allocate memory";
  container_is_open_ = true;
}

void MessageWriter::OpenVariant(const std::string& signature,
                                MessageWriter* writer) {
  DCHECK(!container_is_open_);

  const bool success = dbus_message_iter_open_container(
      &raw_message_iter_, DBUS_TYPE_VARIANT, signature.c_str(),
      &writer->raw_message_iter_);
  CHECK(success) << "Unable to allocate memory";
  container_is_open_ = true;
}

void MessageWriter::OpenStruct(MessageWriter* writer) {
  DCHECK(!container_is_open_);

  const bool success =
      dbus_message_iter_open_container(&raw_message_iter_, DBUS_TYPE_STRUCT,
                                       nullptr,  // Signature should be nullptr.
                                       &writer->raw_message_iter_);
  CHECK(success) << "Unable to allocate memory";
  container_is_open_ = true;
}

void MessageWriter::OpenDictEntry(MessageWriter* writer) {
  DCHECK(!container_is_open_);

  const bool success =
      dbus_message_iter_open_container(&raw_message_iter_, DBUS_TYPE_DICT_ENTRY,
                                       nullptr,  // Signature should be nullptr.
                                       &writer->raw_message_iter_);
  CHECK(success) << "Unable to allocate memory";
  container_is_open_ = true;
}

void MessageWriter::CloseContainer(MessageWriter* writer) {
  DCHECK(container_is_open_);

  const bool success = dbus_message_iter_close_container(
      &raw_message_iter_, &writer->raw_message_iter_);
  CHECK(success) << "Unable to allocate memory";
  container_is_open_ = false;
}

void MessageWriter::AppendArrayOfBytes(base::span<const uint8_t> values) {
  DCHECK(!container_is_open_);
  MessageWriter array_writer(message_);
  OpenArray("y", &array_writer);
  // dbus_message_iter_append_fixed_array takes a pointer to a pointer to the
  // data.
  const uint8_t* ptr = values.data();
  const bool success = dbus_message_iter_append_fixed_array(
      &(array_writer.raw_message_iter_), DBUS_TYPE_BYTE, &ptr,
      base::checked_cast<int>(values.size()));
  CHECK(success) << "Unable to allocate memory";
  CloseContainer(&array_writer);
}

void MessageWriter::AppendArrayOfInt32s(base::span<const int32_t> values) {
  DCHECK(!container_is_open_);
  MessageWriter array_writer(message_);
  OpenArray("i", &array_writer);
  // dbus_message_iter_append_fixed_array takes a pointer to a pointer to the
  // data.
  const int32_t* ptr = values.data();
  const bool success = dbus_message_iter_append_fixed_array(
      &(array_writer.raw_message_iter_), DBUS_TYPE_INT32, &ptr,
      base::checked_cast<int>(values.size()));
  CHECK(success) << "Unable to allocate memory";
  CloseContainer(&array_writer);
}

void MessageWriter::AppendArrayOfUint32s(base::span<const uint32_t> values) {
  DCHECK(!container_is_open_);
  MessageWriter array_writer(message_);
  OpenArray("u", &array_writer);
  // dbus_message_iter_append_fixed_array takes a pointer to a pointer to the
  // data.
  const uint32_t* ptr = values.data();
  const bool success = dbus_message_iter_append_fixed_array(
      &(array_writer.raw_message_iter_), DBUS_TYPE_UINT32, &ptr,
      base::checked_cast<int>(values.size()));
  CHECK(success) << "Unable to allocate memory";
  CloseContainer(&array_writer);
}

void MessageWriter::AppendArrayOfDoubles(base::span<const double> values) {
  DCHECK(!container_is_open_);
  MessageWriter array_writer(message_);
  OpenArray("d", &array_writer);
  // dbus_message_iter_append_fixed_array takes a pointer to a pointer to the
  // data.
  const double* ptr = values.data();
  const bool success = dbus_message_iter_append_fixed_array(
      &(array_writer.raw_message_iter_), DBUS_TYPE_DOUBLE, &ptr,
      base::checked_cast<int>(values.size()));
  CHECK(success) << "Unable to allocate memory";
  CloseContainer(&array_writer);
}

void MessageWriter::AppendArrayOfStrings(
    const std::vector<std::string>& strings) {
  DCHECK(!container_is_open_);
  MessageWriter array_writer(message_);
  OpenArray("s", &array_writer);
  for (size_t i = 0; i < strings.size(); ++i) {
    array_writer.AppendString(strings[i]);
  }
  CloseContainer(&array_writer);
}

void MessageWriter::AppendArrayOfObjectPaths(
    const std::vector<ObjectPath>& object_paths) {
  DCHECK(!container_is_open_);
  MessageWriter array_writer(message_);
  OpenArray("o", &array_writer);
  for (size_t i = 0; i < object_paths.size(); ++i) {
    array_writer.AppendObjectPath(object_paths[i]);
  }
  CloseContainer(&array_writer);
}

bool MessageWriter::AppendProtoAsArrayOfBytes(
    const google::protobuf::MessageLite& protobuf) {
  std::string serialized_proto;
  if (!protobuf.SerializeToString(&serialized_proto)) {
    LOG(ERROR) << "Unable to serialize supplied protocol buffer";
    return false;
  }
  AppendArrayOfBytes(base::as_byte_span(serialized_proto));
  return true;
}

void MessageWriter::AppendVariantOfByte(uint8_t value) {
  AppendVariantOfBasic(DBUS_TYPE_BYTE, &value);
}

void MessageWriter::AppendVariantOfBool(bool value) {
  // See the comment at MessageWriter::AppendBool().
  dbus_bool_t dbus_value = value;
  AppendVariantOfBasic(DBUS_TYPE_BOOLEAN, &dbus_value);
}

void MessageWriter::AppendVariantOfInt16(int16_t value) {
  AppendVariantOfBasic(DBUS_TYPE_INT16, &value);
}

void MessageWriter::AppendVariantOfUint16(uint16_t value) {
  AppendVariantOfBasic(DBUS_TYPE_UINT16, &value);
}

void MessageWriter::AppendVariantOfInt32(int32_t value) {
  AppendVariantOfBasic(DBUS_TYPE_INT32, &value);
}

void MessageWriter::AppendVariantOfUint32(uint32_t value) {
  AppendVariantOfBasic(DBUS_TYPE_UINT32, &value);
}

void MessageWriter::AppendVariantOfInt64(int64_t value) {
  AppendVariantOfBasic(DBUS_TYPE_INT64, &value);
}

void MessageWriter::AppendVariantOfUint64(uint64_t value) {
  AppendVariantOfBasic(DBUS_TYPE_UINT64, &value);
}

void MessageWriter::AppendVariantOfDouble(double value) {
  AppendVariantOfBasic(DBUS_TYPE_DOUBLE, &value);
}

void MessageWriter::AppendVariantOfString(const std::string& value) {
  const char* pointer = value.c_str();
  AppendVariantOfBasic(DBUS_TYPE_STRING, &pointer);
}

void MessageWriter::AppendVariantOfObjectPath(const ObjectPath& value) {
  const char* pointer = value.value().c_str();
  AppendVariantOfBasic(DBUS_TYPE_OBJECT_PATH, &pointer);
}

void MessageWriter::AppendBasic(int dbus_type, const void* value) {
  DCHECK(!container_is_open_);

  const bool success =
      dbus_message_iter_append_basic(&raw_message_iter_, dbus_type, value);
  // dbus_message_iter_append_basic() fails only when there is not enough
  // memory. We don't return this error as there is nothing we can do when
  // it fails to allocate memory for a byte etc.
  CHECK(success) << "Unable to allocate memory";
}

void MessageWriter::AppendVariantOfBasic(int dbus_type, const void* value) {
  const std::string signature(1u,  // length
                              base::checked_cast<char>(dbus_type));
  MessageWriter variant_writer(message_);
  OpenVariant(signature, &variant_writer);
  variant_writer.AppendBasic(dbus_type, value);
  CloseContainer(&variant_writer);
}

void MessageWriter::AppendFileDescriptor(int value) {
  CHECK(IsDBusTypeUnixFdSupported());
  AppendBasic(DBUS_TYPE_UNIX_FD, &value);  // This duplicates the FD.
}

//
// MessageReader implementation.
//

MessageReader::MessageReader(Message* message) : message_(message) {
  memset(&raw_message_iter_, 0, sizeof(raw_message_iter_));
  if (message)
    dbus_message_iter_init(message_->raw_message(), &raw_message_iter_);
}

MessageReader::~MessageReader() = default;

bool MessageReader::HasMoreData() {
  const int dbus_type = dbus_message_iter_get_arg_type(&raw_message_iter_);
  return dbus_type != DBUS_TYPE_INVALID;
}

bool MessageReader::PopByte(uint8_t* value) {
  return PopBasic(DBUS_TYPE_BYTE, value);
}

bool MessageReader::PopBool(bool* value) {
  // Like MessageWriter::AppendBool(), we should copy |value| to
  // dbus_bool_t, as dbus_message_iter_get_basic() used in PopBasic()
  // expects four bytes for DBUS_TYPE_BOOLEAN.
  dbus_bool_t dbus_value = FALSE;
  const bool success = PopBasic(DBUS_TYPE_BOOLEAN, &dbus_value);
  *value = static_cast<bool>(dbus_value);
  return success;
}

bool MessageReader::PopInt16(int16_t* value) {
  return PopBasic(DBUS_TYPE_INT16, value);
}

bool MessageReader::PopUint16(uint16_t* value) {
  return PopBasic(DBUS_TYPE_UINT16, value);
}

bool MessageReader::PopInt32(int32_t* value) {
  return PopBasic(DBUS_TYPE_INT32, value);
}

bool MessageReader::PopUint32(uint32_t* value) {
  return PopBasic(DBUS_TYPE_UINT32, value);
}

bool MessageReader::PopInt64(int64_t* value) {
  return PopBasic(DBUS_TYPE_INT64, value);
}

bool MessageReader::PopUint64(uint64_t* value) {
  return PopBasic(DBUS_TYPE_UINT64, value);
}

bool MessageReader::PopDouble(double* value) {
  return PopBasic(DBUS_TYPE_DOUBLE, value);
}

bool MessageReader::PopString(std::string* value) {
  char* tmp_value = nullptr;
  const bool success = PopBasic(DBUS_TYPE_STRING, &tmp_value);
  if (success)
    value->assign(tmp_value);
  return success;
}

bool MessageReader::PopObjectPath(ObjectPath* value) {
  char* tmp_value = nullptr;
  const bool success = PopBasic(DBUS_TYPE_OBJECT_PATH, &tmp_value);
  if (success)
    *value = ObjectPath(tmp_value);
  return success;
}

bool MessageReader::PopArray(MessageReader* sub_reader) {
  return PopContainer(DBUS_TYPE_ARRAY, sub_reader);
}

bool MessageReader::PopStruct(MessageReader* sub_reader) {
  return PopContainer(DBUS_TYPE_STRUCT, sub_reader);
}

bool MessageReader::PopDictEntry(MessageReader* sub_reader) {
  return PopContainer(DBUS_TYPE_DICT_ENTRY, sub_reader);
}

bool MessageReader::PopVariant(MessageReader* sub_reader) {
  return PopContainer(DBUS_TYPE_VARIANT, sub_reader);
}

bool MessageReader::PopArrayOfBytes(const uint8_t** bytes, size_t* length) {
  MessageReader array_reader(message_);
  if (!PopArray(&array_reader))
    return false;
  // An empty array is allowed.
  if (!array_reader.HasMoreData()) {
    *length = 0;
    *bytes = nullptr;
    return true;
  }
  if (!array_reader.CheckDataType(DBUS_TYPE_BYTE))
    return false;
  int int_length = 0;
  dbus_message_iter_get_fixed_array(&array_reader.raw_message_iter_, bytes,
                                    &int_length);
  *length = static_cast<size_t>(int_length);
  return true;
}

bool MessageReader::PopArrayOfInt32s(const int32_t** signed_ints,
                                     size_t* length) {
  MessageReader array_reader(message_);
  if (!PopArray(&array_reader))
    return false;
  // An empty array is allowed.
  if (!array_reader.HasMoreData()) {
    *length = 0;
    *signed_ints = nullptr;
    return true;
  }
  if (!array_reader.CheckDataType(DBUS_TYPE_INT32))
    return false;
  int int_length = 0;
  dbus_message_iter_get_fixed_array(&array_reader.raw_message_iter_,
                                    signed_ints, &int_length);
  *length = static_cast<size_t>(int_length);
  return true;
}

bool MessageReader::PopArrayOfUint32s(const uint32_t** unsigned_ints,
                                      size_t* length) {
  MessageReader array_reader(message_);
  if (!PopArray(&array_reader))
    return false;
  // An empty array is allowed.
  if (!array_reader.HasMoreData()) {
    *length = 0;
    *unsigned_ints = nullptr;
    return true;
  }
  if (!array_reader.CheckDataType(DBUS_TYPE_UINT32))
    return false;
  int int_length = 0;
  dbus_message_iter_get_fixed_array(&array_reader.raw_message_iter_,
                                    unsigned_ints, &int_length);
  *length = static_cast<size_t>(int_length);
  return true;
}

bool MessageReader::PopArrayOfDoubles(const double** doubles, size_t* length) {
  MessageReader array_reader(message_);
  if (!PopArray(&array_reader))
    return false;
  if (!array_reader.HasMoreData()) {
    *length = 0;
    *doubles = nullptr;
    return true;
  }
  if (!array_reader.CheckDataType(DBUS_TYPE_DOUBLE))
    return false;
  int int_length = 0;
  dbus_message_iter_get_fixed_array(&array_reader.raw_message_iter_, doubles,
                                    &int_length);
  *length = static_cast<size_t>(int_length);
  return true;
}

bool MessageReader::PopArrayOfStrings(std::vector<std::string>* strings) {
  strings->clear();
  MessageReader array_reader(message_);
  if (!PopArray(&array_reader))
    return false;
  while (array_reader.HasMoreData()) {
    std::string string;
    if (!array_reader.PopString(&string))
      return false;
    strings->push_back(string);
  }
  return true;
}

bool MessageReader::PopArrayOfObjectPaths(
    std::vector<ObjectPath>* object_paths) {
  object_paths->clear();
  MessageReader array_reader(message_);
  if (!PopArray(&array_reader))
    return false;
  while (array_reader.HasMoreData()) {
    ObjectPath object_path;
    if (!array_reader.PopObjectPath(&object_path))
      return false;
    object_paths->push_back(object_path);
  }
  return true;
}

bool MessageReader::PopArrayOfBytesAsProto(
    google::protobuf::MessageLite* protobuf) {
  DCHECK(protobuf);
  const char* serialized_buf = nullptr;
  size_t buf_size = 0;
  if (!PopArrayOfBytes(reinterpret_cast<const uint8_t**>(&serialized_buf),
                       &buf_size)) {
    LOG(ERROR) << "Error reading array of bytes";
    return false;
  }
  if (!protobuf->ParseFromArray(serialized_buf, buf_size)) {
    LOG(ERROR) << "Failed to parse protocol buffer from array";
    return false;
  }
  return true;
}

bool MessageReader::PopVariantOfByte(uint8_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_BYTE, value);
}

bool MessageReader::PopVariantOfBool(bool* value) {
  // See the comment at MessageReader::PopBool().
  dbus_bool_t dbus_value = FALSE;
  const bool success = PopVariantOfBasic(DBUS_TYPE_BOOLEAN, &dbus_value);
  *value = static_cast<bool>(dbus_value);
  return success;
}

bool MessageReader::PopVariantOfInt16(int16_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_INT16, value);
}

bool MessageReader::PopVariantOfUint16(uint16_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_UINT16, value);
}

bool MessageReader::PopVariantOfInt32(int32_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_INT32, value);
}

bool MessageReader::PopVariantOfUint32(uint32_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_UINT32, value);
}

bool MessageReader::PopVariantOfInt64(int64_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_INT64, value);
}

bool MessageReader::PopVariantOfUint64(uint64_t* value) {
  return PopVariantOfBasic(DBUS_TYPE_UINT64, value);
}

bool MessageReader::PopVariantOfDouble(double* value) {
  return PopVariantOfBasic(DBUS_TYPE_DOUBLE, value);
}

bool MessageReader::PopVariantOfString(std::string* value) {
  char* tmp_value = nullptr;
  const bool success = PopVariantOfBasic(DBUS_TYPE_STRING, &tmp_value);
  if (success)
    value->assign(tmp_value);
  return success;
}

bool MessageReader::PopVariantOfObjectPath(ObjectPath* value) {
  char* tmp_value = nullptr;
  const bool success = PopVariantOfBasic(DBUS_TYPE_OBJECT_PATH, &tmp_value);
  if (success)
    *value = ObjectPath(tmp_value);
  return success;
}

Message::DataType MessageReader::GetDataType() {
  const int dbus_type = dbus_message_iter_get_arg_type(&raw_message_iter_);
  return static_cast<Message::DataType>(dbus_type);
}

std::string MessageReader::GetDataSignature() {
  std::string signature;
  char* raw_signature = dbus_message_iter_get_signature(&raw_message_iter_);
  if (raw_signature) {
    signature = raw_signature;
    dbus_free(raw_signature);
  }
  return signature;
}

bool MessageReader::CheckDataType(int dbus_type) {
  const int actual_type = dbus_message_iter_get_arg_type(&raw_message_iter_);
  if (actual_type != dbus_type) {
    VLOG(1) << "Type " << dbus_type << " is expected but got " << actual_type;
    return false;
  }
  return true;
}

bool MessageReader::PopBasic(int dbus_type, void* value) {
  if (!CheckDataType(dbus_type))
    return false;
  // dbus_message_iter_get_basic() here should always work, as we have
  // already checked the next item's data type in CheckDataType(). Note
  // that dbus_message_iter_get_basic() is a void function.
  dbus_message_iter_get_basic(&raw_message_iter_, value);
  DCHECK(value);
  dbus_message_iter_next(&raw_message_iter_);
  return true;
}

bool MessageReader::PopContainer(int dbus_type, MessageReader* sub_reader) {
  DCHECK_NE(this, sub_reader);

  if (!CheckDataType(dbus_type))
    return false;
  dbus_message_iter_recurse(&raw_message_iter_, &sub_reader->raw_message_iter_);
  dbus_message_iter_next(&raw_message_iter_);
  return true;
}

bool MessageReader::PopVariantOfBasic(int dbus_type, void* value) {
  MessageReader variant_reader(message_);
  if (!PopVariant(&variant_reader))
    return false;
  return variant_reader.PopBasic(dbus_type, value);
}

bool MessageReader::PopFileDescriptor(base::ScopedFD* value) {
  CHECK(IsDBusTypeUnixFdSupported());

  int fd = -1;
  const bool success = PopBasic(DBUS_TYPE_UNIX_FD, &fd);
  if (!success)
    return false;

  *value = base::ScopedFD(fd);
  return true;
}

}  // namespace dbus
