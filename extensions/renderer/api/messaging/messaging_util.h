// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_MESSAGING_UTIL_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_MESSAGING_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "v8/include/v8-forward.h"

namespace blink {
class WebLocalFrame;
}

namespace extensions {
enum class SerializationFormat;
class ScriptContext;
struct Message;

namespace messaging_util {

// The channel names for the sendMessage and sendRequest calls.
extern const char kSendMessageChannel[];
extern const char kSendRequestChannel[];

// Messaging-related events.
extern const char kOnMessageEvent[];
extern const char kOnMessageExternalEvent[];
extern const char kOnRequestEvent[];
extern const char kOnRequestExternalEvent[];
extern const char kOnConnectEvent[];
extern const char kOnConnectExternalEvent[];
extern const char kOnConnectNativeEvent[];

extern const int kNoFrameId;

// Parses the message from a v8 value, returning null on failure. On error,
// will populate |error_out|.
std::unique_ptr<Message> MessageFromV8(v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value,
                                       SerializationFormat format,
                                       std::string* error);

// Converts a message to a v8 value. This is expected not to fail, since it
// should only be used for messages that have been validated.
v8::Local<v8::Value> MessageToV8(v8::Local<v8::Context> context,
                                 const Message& message);

// Extracts an integer id from |value|, including accounting for -0 (which is a
// valid integer, but is stored in V8 as a number). This will DCHECK that
// |value| is either an int32 or -0.
int ExtractIntegerId(v8::Local<v8::Value> value);

// Returns the preferred serialization format for the given `context`. Note
// extension native messaging clients shouldn't call this as they should always
// use JSON.
SerializationFormat GetSerializationFormat(const ScriptContext& context);

// Flags for ParseMessageOptions().
enum ParseOptionsFlags {
  NO_FLAGS = 0,
  PARSE_CHANNEL_NAME = 1,
  PARSE_FRAME_ID = 1 << 1,
};

struct MessageOptions {
  std::string channel_name;
  int frame_id = kNoFrameId;
  std::string document_id;
};

// Parses and returns the options parameter for sendMessage or connect.
// |flags| specifies additional properties to look for on the object.
MessageOptions ParseMessageOptions(v8::Local<v8::Context> context,
                                   v8::Local<v8::Object> v8_options,
                                   int flags);

// Parses the target from |v8_target_id|, or uses the extension associated with
// the |script_context| as a default. Returns true on success, and false on
// failure. In the case of failure, will populate |error_out| with an error
// based on the |method_name|.
bool GetTargetExtensionId(ScriptContext* script_context,
                          v8::Local<v8::Value> v8_target_id,
                          const char* method_name,
                          std::string* target_out,
                          std::string* error_out);

// Massages the sendMessage() or sendRequest() arguments into the expected
// schema. These arguments are ambiguous (could match multiple signatures), so
// we can't just rely on the normal signature parsing. Sets |arguments| to the
// result if successful; otherwise leaves |arguments| untouched. (If the massage
// is unsuccessful, our normal argument parsing code should throw a reasonable
// error.
void MassageSendMessageArguments(
    v8::Isolate* isolate,
    bool allow_options_argument,
    std::vector<v8::Local<v8::Value>>* arguments_out);

// Returns true if the sendRequest-related properties are disabled for the given
// |script_context|.
bool IsSendRequestDisabled(ScriptContext* script_context);

}  // namespace messaging_util
}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_MESSAGING_UTIL_H_
