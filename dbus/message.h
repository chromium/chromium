// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_MESSAGE_H_
#define DBUS_MESSAGE_H_

#include <dbus/dbus.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "dbus/dbus_export.h"
#include "dbus/object_path.h"

namespace google {
namespace protobuf {

class MessageLite;

}  // namespace protobuf
}  // namespace google

namespace dbus {

class MessageWriter;
class MessageReader;

// DBUS_TYPE_UNIX_FD was added in D-Bus version 1.4
#if !defined(DBUS_TYPE_UNIX_FD)
#define DBUS_TYPE_UNIX_FD ((int)'h')
#endif

// Returns true if Unix FD passing is supported in libdbus.
// The check is done runtime rather than compile time as the libdbus
// version used at runtime may be different from the one used at compile time.
CHROME_DBUS_EXPORT bool IsDBusTypeUnixFdSupported();

// Message is the base class of D-Bus message types. Client code must use
// sub classes such as MethodCall and Response instead.
//
// The class name Message is very generic, but there should be no problem
// as the class is inside 'dbus' namespace. We chose to name this way, as
// libdbus defines lots of types starting with DBus, such as
// DBusMessage. We should avoid confusion and conflict with these types.
class CHROME_DBUS_EXPORT Message {
 public:
  // The message type used in D-Bus.  Redefined here so client code
  // doesn't need to use raw D-Bus macros. DBUS_MESSAGE_TYPE_INVALID
  // etc. are #define macros. Having an enum type here makes code a bit
  // more type-safe.
  enum MessageType {
    MESSAGE_INVALID = DBUS_MESSAGE_TYPE_INVALID,
    MESSAGE_METHOD_CALL = DBUS_MESSAGE_TYPE_METHOD_CALL,
    MESSAGE_METHOD_RETURN = DBUS_MESSAGE_TYPE_METHOD_RETURN,
    MESSAGE_SIGNAL = DBUS_MESSAGE_TYPE_SIGNAL,
    MESSAGE_ERROR = DBUS_MESSAGE_TYPE_ERROR,
  };

  // The data type used in the D-Bus type system.  See the comment at
  // MessageType for why we are redefining data types here.
  enum DataType {
    INVALID_DATA = DBUS_TYPE_INVALID,
    BYTE = DBUS_TYPE_BYTE,
    BOOL = DBUS_TYPE_BOOLEAN,
    INT16 = DBUS_TYPE_INT16,
    UINT16 = DBUS_TYPE_UINT16,
    INT32 = DBUS_TYPE_INT32,
    UINT32 = DBUS_TYPE_UINT32,
    INT64 = DBUS_TYPE_INT64,
    UINT64 = DBUS_TYPE_UINT64,
    DOUBLE = DBUS_TYPE_DOUBLE,
    STRING = DBUS_TYPE_STRING,
    OBJECT_PATH = DBUS_TYPE_OBJECT_PATH,
    ARRAY = DBUS_TYPE_ARRAY,
    STRUCT = DBUS_TYPE_STRUCT,
    DICT_ENTRY = DBUS_TYPE_DICT_ENTRY,
    VARIANT = DBUS_TYPE_VARIANT,
    UNIX_FD = DBUS_TYPE_UNIX_FD,
  };

  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;

  // Returns the type of the message. Returns MESSAGE_INVALID if
  // raw_message_ is NULL.
  MessageType GetMessageType();

  // Returns the type of the message as string like "MESSAGE_METHOD_CALL"
  // for instance.
  std::string GetMessageTypeAsString();

  DBusMessage* raw_message() { return raw_message_; }

  // Sets the destination, the path, the interface, the member, etc.
  bool SetDestination(const std::string& destination);
  bool SetPath(const ObjectPath& path);
  bool SetInterface(const std::string& interface);
  bool SetMember(const std::string& member);
  bool SetErrorName(const std::string& error_name);
  bool SetSender(const std::string& sender);
  void SetSerial(uint32_t serial);
  void SetReplySerial(uint32_t reply_serial);
  // SetSignature() does not exist as we cannot do it.

  // Gets the destination, the path, the interface, the member, etc.
  // If not set, an empty string is returned.
  std::string GetDestination();
  ObjectPath GetPath();
  std::string GetInterface();
  std::string GetMember();
  std::string GetErrorName();
  std::string GetSender();
  std::string GetSignature();
  // Gets the serial and reply serial numbers. Returns 0 if not set.
  uint32_t GetSerial();
  uint32_t GetReplySerial();

  // Returns the string representation of this message. Useful for
  // debugging. The output is truncated as needed (ex. strings are truncated
  // if longer than a certain limit defined in the .cc file).
  std::string ToString();

 protected:
  // This class cannot be instantiated. Use sub classes instead.
  Message();
  virtual ~Message();

  // Initializes the message with the given raw message.
  void Init(DBusMessage* raw_message);

 private:
  // Helper function used in ToString().
  std::string ToStringInternal(const std::string& indent,
                               MessageReader* reader);

  raw_ptr<DBusMessage, AcrossTasksDanglingUntriaged> raw_message_;
};

// MessageCall is a type of message used for calling a method via D-Bus.
class CHROME_DBUS_EXPORT MethodCall : public Message {
 public:
  // Creates a method call message for the specified interface name and
  // the method name.
  //
  // For instance, to call "Get" method of DBUS_INTERFACE_INTROSPECTABLE
  // interface ("org.freedesktop.DBus.Introspectable"), create a method
  // call like this:
  //
  //   MethodCall method_call(DBUS_INTERFACE_INTROSPECTABLE, "Get");
  //
  // The constructor creates the internal raw message.
  MethodCall(const std::string& interface_name, const std::string& method_name);

  MethodCall(const MethodCall&) = delete;
  MethodCall& operator=(const MethodCall&) = delete;

  // Returns a newly created MethodCall from the given raw message of the
  // type DBUS_MESSAGE_TYPE_METHOD_CALL. Takes the ownership of |raw_message|.
  static std::unique_ptr<MethodCall> FromRawMessage(DBusMessage* raw_message);

 private:
  // Creates a method call message. The internal raw message is NULL.
  // Only used internally.
  MethodCall();
};

// Signal is a type of message used to send a signal.
class CHROME_DBUS_EXPORT Signal : public Message {
 public:
  // Creates a signal message for the specified interface name and the
  // method name.
  //
  // For instance, to send "PropertiesChanged" signal of
  // DBUS_INTERFACE_INTROSPECTABLE interface
  // ("org.freedesktop.DBus.Introspectable"), create a signal like this:
  //
  //   Signal signal(DBUS_INTERFACE_INTROSPECTABLE, "PropertiesChanged");
  //
  // The constructor creates the internal raw_message_.
  Signal(const std::string& interface_name, const std::string& method_name);

  Signal(const Signal&) = delete;
  Signal& operator=(const Signal&) = delete;

  // Returns a newly created SIGNAL from the given raw message of the type
  // DBUS_MESSAGE_TYPE_SIGNAL. Takes the ownership of |raw_message|.
  static std::unique_ptr<Signal> FromRawMessage(DBusMessage* raw_message);

 private:
  // Creates a signal message. The internal raw message is NULL.
  // Only used internally.
  Signal();
};

// Response is a type of message used for receiving a response from a
// method via D-Bus.
class CHROME_DBUS_EXPORT Response : public Message {
 public:
  // Returns a newly created Response from the given raw message of the
  // type DBUS_MESSAGE_TYPE_METHOD_RETURN. Takes the ownership of |raw_message|.
  static std::unique_ptr<Response> FromRawMessage(DBusMessage* raw_message);

  // Returns a newly created Response from the given method call.
  // Used for implementing exported methods. Does NOT take the ownership of
  // |method_call|.
  static std::unique_ptr<Response> FromMethodCall(MethodCall* method_call);

  // Returns a newly created Response with an empty payload.
  // Useful for testing.
  static std::unique_ptr<Response> CreateEmpty();

  Response(const Response&) = delete;
  Response& operator=(const Response&) = delete;

 protected:
  // Creates a Response message. The internal raw message is NULL.
  Response();
};

// ErrorResponse is a type of message used to return an error to the
// caller of a method.
class CHROME_DBUS_EXPORT ErrorResponse : public Response {
 public:
  // Returns a newly created Response from the given raw message of the
  // type DBUS_MESSAGE_TYPE_METHOD_RETURN. Takes the ownership of |raw_message|.
  static std::unique_ptr<ErrorResponse> FromRawMessage(
      DBusMessage* raw_message);

  // Returns a newly created ErrorResponse from the given method call, the
  // error name, and the error message.  The error name looks like
  // "org.freedesktop.DBus.Error.Failed". Used for returning an error to a
  // failed method call. Does NOT take the ownership of |method_call|.
  static std::unique_ptr<ErrorResponse> FromMethodCall(
      MethodCall* method_call,
      const std::string& error_name,
      const std::string& error_message);

  ErrorResponse(const ErrorResponse&) = delete;
  ErrorResponse& operator=(const ErrorResponse&) = delete;

 private:
  // Creates an ErrorResponse message. The internal raw message is NULL.
  ErrorResponse();
};

// MessageWriter is used to write outgoing messages for calling methods
// and sending signals.
//
// The main design goal of MessageReader and MessageWriter classes is to
// provide a type safe API. In the past, there was a Chrome OS blocker
// bug, that took days to fix, that would have been prevented if the API
// was type-safe.
//
// For instance, instead of doing something like:
//
//   // We shouldn't add '&' to str here, but it compiles with '&' added.
//   dbus_g_proxy_call(..., G_TYPE_STRING, str, G_TYPE_INVALID, ...)
//
// We want to do something like:
//
//   writer.AppendString(str);
//
class CHROME_DBUS_EXPORT MessageWriter {
 public:
  // Data added with Append* will be written to |message|, which may be NULL
  // to create a sub-writer for passing to OpenArray, etc.
  explicit MessageWriter(Message* message);

  MessageWriter(const MessageWriter&) = delete;
  MessageWriter& operator=(const MessageWriter&) = delete;

  ~MessageWriter();

  // Appends a byte to the message.
  void AppendByte(uint8_t value);
  void AppendBool(bool value);
  void AppendInt16(int16_t value);
  void AppendUint16(uint16_t value);
  void AppendInt32(int32_t value);
  void AppendUint32(uint32_t value);
  void AppendInt64(int64_t value);
  void AppendUint64(uint64_t value);
  void AppendDouble(double value);
  // This function intentionally takes a `std::string` to ensure the data that
  // should be appended is correctly null-terminated; other data types, e.g.
  // `std::string_view`, do not necessarily have the same end as the
  // `std::string` that owns the underlying data. This can result in more data
  // than intended being appended since the end of the `std::string` is used
  // instead of the end of the `std::string_view`.
  void AppendString(const std::string& value);
  void AppendObjectPath(const ObjectPath& value);

  // Appends a file descriptor to the message.
  // The FD will be duplicated so you still have to close the original FD.
  void AppendFileDescriptor(int value);

  // Opens an array. The array contents can be added to the array with
  // |sub_writer|. The client code must close the array with
  // CloseContainer(), once all contents are added.
  //
  // |signature| parameter is used to supply the D-Bus type signature of
  // the array contents. For instance, if you want an array of strings,
  // then you pass "s" as the signature.
  //
  // See the spec for details about the type signatures.
  // http://dbus.freedesktop.org/doc/dbus-specification.html
  // #message-protocol-signatures
  //
  void OpenArray(const std::string& signature, MessageWriter* sub_writer);
  // Do the same for a variant.
  void OpenVariant(const std::string& signature, MessageWriter* sub_writer);
  // Do the same for Struct and dict entry. They don't need the signature.
  void OpenStruct(MessageWriter* sub_writer);
  void OpenDictEntry(MessageWriter* sub_writer);

  // Close the container for a array/variant/struct/dict entry.
  void CloseContainer(MessageWriter* sub_writer);

  // Appends the array of bytes. Arrays of bytes are often used for
  // exchanging binary blobs hence it's worth having a specialized
  // function.
  void AppendArrayOfBytes(base::span<const uint8_t> values);

  // Appends array of int32_ts.
  void AppendArrayOfInt32s(base::span<const int32_t> values);

  // Appends array of uint32_ts.
  void AppendArrayOfUint32s(base::span<const uint32_t> values);

  // Appends the array of doubles. Used for audio mixer matrix doubles.
  void AppendArrayOfDoubles(base::span<const double> values);

  // Appends the array of strings. Arrays of strings are often used for
  // exchanging lists of names hence it's worth having a specialized
  // function.
  void AppendArrayOfStrings(const std::vector<std::string>& strings);

  // Appends the array of object paths. Arrays of object paths are often
  // used when exchanging object paths, hence it's worth having a
  // specialized function.
  void AppendArrayOfObjectPaths(const std::vector<ObjectPath>& object_paths);

  // Appends the protocol buffer as an array of bytes. The buffer is serialized
  // into an array of bytes before communication, since protocol buffers are not
  // a native dbus type. On the receiving size the array of bytes needs to be
  // read and deserialized into a protocol buffer of the correct type. There are
  // methods in MessageReader to assist in this.  Return true on success and
  // false when serialization fails.
  bool AppendProtoAsArrayOfBytes(const google::protobuf::MessageLite& protobuf);

  // Appends the byte wrapped in a variant data container. Variants are
  // widely used in D-Bus services so it's worth having a specialized
  // function. For instance, The third parameter of
  // "org.freedesktop.DBus.Properties.Set" is a variant.
  void AppendVariantOfByte(uint8_t value);
  void AppendVariantOfBool(bool value);
  void AppendVariantOfInt16(int16_t value);
  void AppendVariantOfUint16(uint16_t value);
  void AppendVariantOfInt32(int32_t value);
  void AppendVariantOfUint32(uint32_t value);
  void AppendVariantOfInt64(int64_t value);
  void AppendVariantOfUint64(uint64_t value);
  void AppendVariantOfDouble(double value);
  void AppendVariantOfString(const std::string& value);
  void AppendVariantOfObjectPath(const ObjectPath& value);

 private:
  // Helper function used to implement AppendByte etc.
  void AppendBasic(int dbus_type, const void* value);

  // Helper function used to implement AppendVariantOfByte() etc.
  void AppendVariantOfBasic(int dbus_type, const void* value);

  raw_ptr<Message, AcrossTasksDanglingUntriaged> message_;
  DBusMessageIter raw_message_iter_;
  bool container_is_open_;
};

// MessageReader is used to read incoming messages such as responses for
// method calls.
//
// MessageReader manages an internal iterator to read data. All functions
// starting with Pop advance the iterator on success.
class CHROME_DBUS_EXPORT MessageReader {
 public:
  // The data will be read from the given |message|, which may be NULL to
  // create a sub-reader for passing to PopArray, etc.
  explicit MessageReader(Message* message);

  MessageReader(const MessageReader&) = delete;
  MessageReader& operator=(const MessageReader&) = delete;

  ~MessageReader();

  // Returns true if the reader has more data to read. The function is
  // used to iterate contents in a container like:
  //
  //   while (reader.HasMoreData())
  //     reader.PopString(&value);
  bool HasMoreData();

  // Gets the byte at the current iterator position.
  // Returns true and advances the iterator on success.
  // Returns false if the data type is not a byte.
  bool PopByte(uint8_t* value);
  bool PopBool(bool* value);
  bool PopInt16(int16_t* value);
  bool PopUint16(uint16_t* value);
  bool PopInt32(int32_t* value);
  bool PopUint32(uint32_t* value);
  bool PopInt64(int64_t* value);
  bool PopUint64(uint64_t* value);
  bool PopDouble(double* value);
  bool PopString(std::string* value);
  bool PopObjectPath(ObjectPath* value);
  bool PopFileDescriptor(base::ScopedFD* value);

  // Sets up the given message reader to read an array at the current
  // iterator position.
  // Returns true and advances the iterator on success.
  // Returns false if the data type is not an array
  bool PopArray(MessageReader* sub_reader);
  bool PopStruct(MessageReader* sub_reader);
  bool PopDictEntry(MessageReader* sub_reader);
  bool PopVariant(MessageReader* sub_reader);

  // Gets the array of bytes at the current iterator position.
  // Returns true and advances the iterator on success.
  //
  // Arrays of bytes are often used for exchanging binary blobs hence it's
  // worth having a specialized function.
  //
  // Ownership of the memory pointed to by |bytes| remains with the
  // MessageReader; |bytes| must be copied if the contents will be referenced
  // after the MessageReader is destroyed.
  bool PopArrayOfBytes(const uint8_t** bytes, size_t* length);

  // Gets the array of int32_ts at the current iterator position.
  bool PopArrayOfInt32s(const int32_t** signed_ints, size_t* length);

  // Gets the array of uint32_ts at the current iterator position.
  bool PopArrayOfUint32s(const uint32_t** unsigned_ints, size_t* length);

  // Gets the array of doubles at the current iterator position.
  bool PopArrayOfDoubles(const double** doubles, size_t* length);

  // Gets the array of strings at the current iterator position. |strings| is
  // cleared before being modified. Returns true and advances the iterator on
  // success.
  //
  // Arrays of strings are often used to communicate with D-Bus
  // services like KWallet, hence it's worth having a specialized
  // function.
  bool PopArrayOfStrings(std::vector<std::string>* strings);

  // Gets the array of object paths at the current iterator position.
  // |object_paths| is cleared before being modified. Returns true and advances
  // the iterator on success.
  //
  // Arrays of object paths are often used to communicate with D-Bus
  // services like NetworkManager, hence it's worth having a specialized
  // function.
  bool PopArrayOfObjectPaths(std::vector<ObjectPath>* object_paths);

  // Gets the array of bytes at the current iterator position. It then parses
  // this binary blob into the protocol buffer supplied.
  // Returns true and advances the iterator on success. On failure returns false
  // and emits an error message on the source of the failure. The two most
  // common errors come from the iterator not currently being at a byte array or
  // the wrong type of protocol buffer is passed in and the parse fails.
  bool PopArrayOfBytesAsProto(google::protobuf::MessageLite* protobuf);

  // Gets the byte from the variant data container at the current iterator
  // position.
  // Returns true and advances the iterator on success.
  //
  // Variants are widely used in D-Bus services so it's worth having a
  // specialized function. For instance, The return value type of
  // "org.freedesktop.DBus.Properties.Get" is a variant.
  bool PopVariantOfByte(uint8_t* value);
  bool PopVariantOfBool(bool* value);
  bool PopVariantOfInt16(int16_t* value);
  bool PopVariantOfUint16(uint16_t* value);
  bool PopVariantOfInt32(int32_t* value);
  bool PopVariantOfUint32(uint32_t* value);
  bool PopVariantOfInt64(int64_t* value);
  bool PopVariantOfUint64(uint64_t* value);
  bool PopVariantOfDouble(double* value);
  bool PopVariantOfString(std::string* value);
  bool PopVariantOfObjectPath(ObjectPath* value);

  // Get the data type of the value at the current iterator
  // position. INVALID_DATA will be returned if the iterator points to the
  // end of the message.
  Message::DataType GetDataType();

  // Get the DBus signature of the value at the current iterator position.
  // An empty string will be returned if the iterator points to the end of
  // the message.
  std::string GetDataSignature();

 private:
  // Returns true if the data type at the current iterator position
  // matches the given D-Bus type, such as DBUS_TYPE_BYTE.
  bool CheckDataType(int dbus_type);

  // Helper function used to implement PopByte() etc.
  bool PopBasic(int dbus_type, void* value);

  // Helper function used to implement PopArray() etc.
  bool PopContainer(int dbus_type, MessageReader* sub_reader);

  // Helper function used to implement PopVariantOfByte() etc.
  bool PopVariantOfBasic(int dbus_type, void* value);

  raw_ptr<Message, AcrossTasksDanglingUntriaged> message_;
  DBusMessageIter raw_message_iter_;
};

}  // namespace dbus

#endif  // DBUS_MESSAGE_H_
