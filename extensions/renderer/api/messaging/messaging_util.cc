// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/messaging/messaging_util.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"
#include "extensions/renderer/extension_interaction_provider.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/worker_thread_util.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-exception.h"
#include "v8/include/v8-json.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {
namespace messaging_util {

namespace {

constexpr char kExtensionIdRequiredErrorTemplate[] =
    "chrome.%s() called from a webpage must specify an "
    "Extension ID (string) for its first argument.";

constexpr char kErrorCouldNotSerialize[] = "Could not serialize message.";

constexpr char kErrorMalformedJSONMessage[] =
    "The sender sent an invalid JSON message; message ignored.";

std::unique_ptr<Message> MessageFromJSONString(v8::Isolate* isolate,
                                               v8::Local<v8::Context> context,
                                               v8::Local<v8::String> json,
                                               std::string* error_out,
                                               blink::WebLocalFrame* web_frame,
                                               bool privileged_context) {
  std::string message;
  message = gin::V8ToString(isolate, json);
  // JSON.stringify can fail to produce a string value in one of two ways: it
  // can throw an exception (as with unserializable objects), or it can return
  // `undefined` (as with e.g. passing a function). If JSON.stringify returns
  // `undefined`, the v8 API then coerces it to the string value "undefined".
  // Check for this, and consider it a failure (since we didn't properly
  // serialize a value).
  if (message == "undefined") {
    *error_out = kErrorCouldNotSerialize;
    return nullptr;
  }

  size_t message_length = message.length();

  // IPC messages will fail at > 128 MB. Restrict extension messages to 64 MB.
  // A 64 MB JSON-ifiable object is scary enough as is.
  static constexpr size_t kMaxMessageLength = 1024 * 1024 * 64;
  if (message_length > kMaxMessageLength) {
    *error_out = "Message length exceeded maximum allowed length.";
    return nullptr;
  }

  // Check if there's an active user gesture.
  // For service workers, use the ExtensionInteractionProvider, since there is
  // no associated render frame (and we synthesize user gestures). Otherwise,
  // check the render frame.
  // TODO(https://crbug.com/326889650): Ideally, we'd just check
  // ExtensionInteractionProvider here, because that also knows how to look for
  // user gestures on frame-based contexts. However, one additional check was
  // added here, `LastActivationWasRestricted()`, that isn't in
  // ExtensionInteractionProvider. We should move that check to
  // ExtensionInteractionProvider and then just use that for all gestures
  // checks.
  bool has_unrestricted_user_activation = false;
  if (worker_thread_util::IsWorkerThread()) {
    has_unrestricted_user_activation =
        ExtensionInteractionProvider::HasActiveExtensionInteraction(context);
  } else {
    // The message should carry user activation information only if the last
    // activation in |web_frame| was triggered by a real user interaction.  See
    // |UserActivationState::LastActivationWasRestricted()|.
    has_unrestricted_user_activation =
        web_frame && web_frame->HasTransientUserActivation() &&
        !web_frame->LastActivationWasRestricted();
  }
  return std::make_unique<Message>(message, mojom::SerializationFormat::kJson,
                                   has_unrestricted_user_activation,
                                   privileged_context);
}

}  // namespace

const char kSendMessageChannel[] = "chrome.runtime.sendMessage";
const char kSendRequestChannel[] = "chrome.extension.sendRequest";

const char kOnMessageEvent[] = "runtime.onMessage";
const char kOnUserScriptMessageEvent[] = "runtime.onUserScriptMessage";
const char kOnMessageExternalEvent[] = "runtime.onMessageExternal";
const char kOnRequestEvent[] = "extension.onRequest";
const char kOnRequestExternalEvent[] = "extension.onRequestExternal";
const char kOnConnectEvent[] = "runtime.onConnect";
const char kOnUserScriptConnectEvent[] = "runtime.onUserScriptConnect";
const char kOnConnectExternalEvent[] = "runtime.onConnectExternal";
const char kOnConnectNativeEvent[] = "runtime.onConnectNative";

const int kNoFrameId = -1;

std::unique_ptr<Message> MessageFromV8(v8::Local<v8::Context> context,
                                       v8::Local<v8::Value> value,
                                       mojom::SerializationFormat format,
                                       std::string* error_out) {
  // TODO(crbug.com/40321352): Incorporate `format` while serializing the
  // message.
  DCHECK(!value.IsEmpty());
  v8::Isolate* isolate = context->GetIsolate();
  v8::Context::Scope context_scope(context);

  // TODO(devlin): For some reason, we don't use the signature for
  // Port.postMessage when evaluating the parameters. We probably should, but
  // we don't know how many extensions that may break. It would be good to
  // investigate, and, ideally, use the signature.

  if (value->IsUndefined()) {
    // JSON.stringify won't serialized undefined (it returns undefined), but it
    // will serialized null. We've always converted undefined to null in JS
    // bindings, so preserve this behavior for now.
    value = v8::Null(isolate);
  }

  bool success = false;
  v8::Local<v8::String> stringified;
  {
    v8::TryCatch try_catch(isolate);
    success = v8::JSON::Stringify(context, value).ToLocal(&stringified);
  }

  if (!success) {
    *error_out = kErrorCouldNotSerialize;
    return nullptr;
  }

  ScriptContext* script_context = GetScriptContextFromV8Context(context);
  blink::WebLocalFrame* web_frame =
      script_context ? script_context->web_frame() : nullptr;
  bool privileged_context =
      script_context &&
      script_context->context_type() ==
          extensions::mojom::ContextType::kPrivilegedExtension;
  return MessageFromJSONString(isolate, context, stringified, error_out,
                               web_frame, privileged_context);
}

v8::Local<v8::Value> MessageToV8(v8::Local<v8::Context> context,
                                 const Message& message,
                                 bool is_parsing_fail_safe,
                                 std::string* error) {
  // TODO(crbug.com/40321352): Incorporate `message.format` while deserializing
  // the message.

  v8::Isolate* isolate = context->GetIsolate();
  v8::Context::Scope context_scope(context);

  v8::Local<v8::String> v8_message_string =
      gin::StringToV8(isolate, message.data);
  v8::Local<v8::Value> parsed_message;
  v8::TryCatch try_catch(isolate);
  if (!v8::JSON::Parse(context, v8_message_string).ToLocal(&parsed_message)) {
    CHECK(is_parsing_fail_safe);
    if (error) {
      *error = kErrorMalformedJSONMessage;
    }
    return v8::Local<v8::Value>();
  }
  return parsed_message;
}

int ExtractIntegerId(v8::Local<v8::Value> value) {
  if (value->IsInt32())
    return value.As<v8::Int32>()->Value();

  // Account for -0, which is a valid integer, but is stored as a number in v8.
  DCHECK(value->IsNumber() && value.As<v8::Number>()->Value() == 0.0);
  return 0;
}

mojom::SerializationFormat GetSerializationFormat(
    const ScriptContext& script_context) {
  if (!base::FeatureList::IsEnabled(
          extensions_features::kStructuredCloningForMV3Messaging)) {
    return mojom::SerializationFormat::kJson;
  }

  const Extension* extension = script_context.extension();
  return extension && extension->manifest_version() >= 3
             ? mojom::SerializationFormat::kStructuredCloned
             : mojom::SerializationFormat::kJson;
}

MessageOptions ParseMessageOptions(v8::Local<v8::Context> context,
                                   v8::Local<v8::Object> v8_options,
                                   int flags) {
  DCHECK(!v8_options.IsEmpty());
  DCHECK(!v8_options->IsNull());

  v8::Isolate* isolate = context->GetIsolate();

  MessageOptions options;

  gin::Dictionary options_dict(isolate, v8_options);
  if ((flags & PARSE_CHANNEL_NAME) != 0) {
    v8::Local<v8::Value> v8_channel_name;
    bool success = options_dict.Get("name", &v8_channel_name);
    DCHECK(success);

    if (!v8_channel_name->IsUndefined()) {
      DCHECK(v8_channel_name->IsString());
      options.channel_name = gin::V8ToString(isolate, v8_channel_name);
    }
  }

  if ((flags & PARSE_FRAME_ID) != 0) {
    v8::Local<v8::Value> v8_frame_id;
    bool success = options_dict.Get("frameId", &v8_frame_id);
    DCHECK(success);

    if (!v8_frame_id->IsUndefined()) {
      DCHECK(v8_frame_id->IsInt32());
      int frame_id = v8_frame_id.As<v8::Int32>()->Value();
      // NOTE(devlin): JS bindings coerce any negative value to -1. For
      // backwards compatibility, we do the same here.
      options.frame_id = frame_id < 0 ? -1 : frame_id;
    }

    v8::Local<v8::Value> v8_document_id;
    success = options_dict.Get("documentId", &v8_document_id);
    DCHECK(success);

    if (!v8_document_id->IsUndefined()) {
      DCHECK(v8_document_id->IsString());
      options.document_id = gin::V8ToString(isolate, v8_document_id);
    }
  }

  // Note: the options object may also include an includeTlsChannelId property.
  // That property has been a no-op since M72. See crbug.com/1045232.
  return options;
}

bool GetTargetExtensionId(ScriptContext* script_context,
                          v8::Local<v8::Value> v8_target_id,
                          const char* method_name,
                          std::string* target_out,
                          std::string* error_out) {
  DCHECK(!v8_target_id.IsEmpty());
  // Argument parsing should guarantee this is null or a string before we reach
  // this point.
  DCHECK(v8_target_id->IsNull() || v8_target_id->IsString());

  std::string target_id;
  // If omitted, we use the extension associated with the context.
  // Note: we deliberately treat the empty string as omitting the id, even
  // though it's not strictly correct. See https://crbug.com/823577.
  if (v8_target_id->IsNull() ||
      (v8_target_id->IsString() &&
       v8_target_id.As<v8::String>()->Length() == 0)) {
    if (!script_context->extension()) {
      *error_out =
          base::StringPrintf(kExtensionIdRequiredErrorTemplate, method_name);
      return false;
    }

    target_id = script_context->extension()->id();
    // An extension should never have an invalid id.
    DCHECK(crx_file::id_util::IdIsValid(target_id));
  } else {
    DCHECK(v8_target_id->IsString());
    target_id = gin::V8ToString(script_context->isolate(), v8_target_id);
    // NOTE(devlin): JS bindings only validate that the extension id is present,
    // rather than validating its content. This seems better. Let's see how this
    // goes.
    if (!crx_file::id_util::IdIsValid(target_id)) {
      *error_out =
          base::StringPrintf("Invalid extension id: '%s'", target_id.c_str());
      return false;
    }
  }

  if (script_context->context_type() == mojom::ContextType::kUserScript) {
    // User scripts should *always* have an associated extension.
    CHECK(script_context->extension());
    if (script_context->extension()->id() != target_id) {
      *error_out = "User scripts may not message external extensions.";
      return false;
    }
  }

  *target_out = std::move(target_id);
  return true;
}

void MassageSendMessageArguments(v8::Isolate* isolate,
                                 bool allow_options_argument,
                                 v8::LocalVector<v8::Value>* arguments_out) {
  base::span<const v8::Local<v8::Value>> arguments = *arguments_out;
  size_t max_size = allow_options_argument ? 4u : 3u;
  if (arguments.empty() || arguments.size() > max_size)
    return;

  v8::Local<v8::Value> target_id = v8::Null(isolate);
  v8::Local<v8::Value> message = v8::Null(isolate);
  v8::Local<v8::Value> options;
  if (allow_options_argument)
    options = v8::Null(isolate);
  v8::Local<v8::Value> response_callback = v8::Null(isolate);

  // If the last argument is a function, it is the response callback.
  // Ignore it for the purposes of further argument parsing.
  if ((*arguments.rbegin())->IsFunction()) {
    response_callback = *arguments.rbegin();
    arguments = arguments.first(arguments.size() - 1);
  }

  // Re-check for too many arguments after looking for the callback. If there
  // are, early-out and rely on normal signature parsing to report the error.
  if (arguments.size() >= max_size)
    return;

  switch (arguments.size()) {
    case 0:
      // Required argument (message) is missing.
      // Early-out and rely on normal signature parsing to report this error.
      return;
    case 1:
      // Argument must be the message.
      message = arguments[0];
      break;
    case 2: {
      // Assume the first argument is the ID if we don't expect options, or if
      // the argument could match the ID parameter.
      // ID could be either a string, or null/undefined (since it's optional).
      bool could_match_id =
          arguments[0]->IsString() || arguments[0]->IsNullOrUndefined();
      if (!allow_options_argument || could_match_id) {
        target_id = arguments[0];
        message = arguments[1];
      } else {  // Otherwise, the meaning is (message, options).
        message = arguments[0];
        options = arguments[1];
      }
      break;
    }
    case 3:
      DCHECK(allow_options_argument);
      // The meaning in this case is unambiguous.
      target_id = arguments[0];
      message = arguments[1];
      options = arguments[2];
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  if (allow_options_argument)
    *arguments_out = {target_id, message, options, response_callback};
  else
    *arguments_out = {target_id, message, response_callback};
}

bool IsSendRequestDisabled(ScriptContext* script_context) {
  const Extension* extension = script_context->extension();
  return extension && Manifest::IsUnpackedLocation(extension->location()) &&
         BackgroundInfo::HasLazyBackgroundPage(extension);
}

std::string GetEventForChannel(const MessagingEndpoint& source_endpoint,
                               const ExtensionId& target_extension_id,
                               mojom::ChannelType channel_type) {
  bool is_external_event =
      MessagingEndpoint::IsExternal(source_endpoint, target_extension_id);
  bool is_user_script_event =
      source_endpoint.type == MessagingEndpoint::Type::kUserScript;
  // We (deliberately) do not support external user script events.
  CHECK(!is_user_script_event || !is_external_event);

  std::string event_name;
  switch (channel_type) {
    case mojom::ChannelType::kSendRequest:
      CHECK(!is_user_script_event);
      event_name = is_external_event ? messaging_util::kOnRequestExternalEvent
                                     : messaging_util::kOnRequestEvent;
      break;
    case mojom::ChannelType::kSendMessage:
      if (is_external_event) {
        event_name = messaging_util::kOnMessageExternalEvent;
      } else if (is_user_script_event) {
        event_name = messaging_util::kOnUserScriptMessageEvent;
      } else {
        event_name = messaging_util::kOnMessageEvent;
      }
      break;
    case mojom::ChannelType::kConnect:
      if (is_external_event) {
        event_name = messaging_util::kOnConnectExternalEvent;
      } else if (is_user_script_event) {
        event_name = messaging_util::kOnUserScriptConnectEvent;
      } else {
        event_name = messaging_util::kOnConnectEvent;
      }
      break;
    case mojom::ChannelType::kNative:
      CHECK(!is_user_script_event);
      event_name = messaging_util::kOnConnectNativeEvent;
      break;
  }

  return event_name;
}

}  // namespace messaging_util
}  // namespace extensions
