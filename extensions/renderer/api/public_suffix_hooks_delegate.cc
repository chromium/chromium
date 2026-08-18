// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/public_suffix_hooks_delegate.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "extensions/renderer/api/public_suffix_util.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

constexpr char kIsKnownSuffix[] = "publicSuffix.isKnownSuffix";
constexpr char kGetKnownSuffix[] = "publicSuffix.getKnownSuffix";
constexpr char kGetDomain[] = "publicSuffix.getDomain";

using api::public_suffix::DomainOptions;
using api::public_suffix::ParsedHostname;
using RequestResult = APIBindingHooks::RequestResult;

v8::Local<v8::Value> OptionalStringToV8(
    v8::Isolate* isolate,
    const std::optional<std::string>& value) {
  if (!value.has_value()) {
    return v8::Null(isolate);
  }
  return gin::StringToV8(isolate, *value);
}

// Note: The arguments have already been validated against the API's schema, so
//       the values just need to be read here.
DomainOptions ParseDomainOptions(v8::Isolate* isolate,
                                 v8::Local<v8::Value> v8_options) {
  DomainOptions options;
  if (v8_options->IsNull()) {
    return options;
  }

  gin::Dictionary options_dict(isolate, v8_options.As<v8::Object>());
  v8::Local<v8::Value> v8_encoding;
  v8::Local<v8::Value> v8_allow_ip_address;
  v8::Local<v8::Value> v8_allow_plain_suffix;
  v8::Local<v8::Value> v8_allow_unknown_suffix;
  if (!options_dict.Get("encoding", &v8_encoding) ||
      !options_dict.Get("allowIPAddress", &v8_allow_ip_address) ||
      !options_dict.Get("allowPlainSuffix", &v8_allow_plain_suffix) ||
      !options_dict.Get("allowUnknownSuffix", &v8_allow_unknown_suffix)) {
    NOTREACHED();
  }

  if (!v8_encoding->IsUndefined()) {
    DCHECK(v8_encoding->IsString());
    options.encoding = api::public_suffix::ParseDomainEncoding(
        gin::V8ToString(isolate, v8_encoding));
  }

  if (!v8_allow_ip_address->IsUndefined()) {
    DCHECK(v8_allow_ip_address->IsBoolean());
    options.allow_ip_address = v8_allow_ip_address.As<v8::Boolean>()->Value();
  }

  if (!v8_allow_plain_suffix->IsUndefined()) {
    DCHECK(v8_allow_plain_suffix->IsBoolean());
    options.allow_plain_suffix =
        v8_allow_plain_suffix.As<v8::Boolean>()->Value();
  }

  if (!v8_allow_unknown_suffix->IsUndefined()) {
    DCHECK(v8_allow_unknown_suffix->IsBoolean());
    options.allow_unknown_suffix =
        v8_allow_unknown_suffix.As<v8::Boolean>()->Value();
  }

  return options;
}

}  // namespace

PublicSuffixHooksDelegate::PublicSuffixHooksDelegate() = default;
PublicSuffixHooksDelegate::~PublicSuffixHooksDelegate() = default;

RequestResult PublicSuffixHooksDelegate::HandleRequest(
    const std::string& method_name,
    const APISignature* signature,
    v8::Local<v8::Context> context,
    v8::LocalVector<v8::Value>* arguments,
    const APITypeReferenceMap& refs) {
  using Handler = RequestResult (PublicSuffixHooksDelegate::*)(
      ScriptContext*, const APISignature::V8ParseResult&);
  static constexpr struct {
    Handler handler;
    std::string_view method;
  } kHandlers[] = {
      {&PublicSuffixHooksDelegate::HandleIsKnownSuffix, kIsKnownSuffix},
      {&PublicSuffixHooksDelegate::HandleGetKnownSuffix, kGetKnownSuffix},
      {&PublicSuffixHooksDelegate::HandleGetDomain, kGetDomain},
  };

  Handler handler = nullptr;
  for (const auto& handler_entry : kHandlers) {
    if (handler_entry.method == method_name) {
      handler = handler_entry.handler;
      break;
    }
  }

  if (!handler) {
    return RequestResult(RequestResult::NOT_HANDLED);
  }

  APISignature::V8ParseResult parse_result =
      signature->ParseArgumentsToV8(context, *arguments, refs);
  if (!parse_result.succeeded()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = std::move(*parse_result.error);
    return result;
  }

  ScriptContext* script_context = GetScriptContextFromV8ContextChecked(context);
  return (this->*handler)(script_context, parse_result);
}

RequestResult PublicSuffixHooksDelegate::HandleIsKnownSuffix(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  DCHECK(arguments[0]->IsString());

  v8::Isolate* isolate = script_context->isolate();
  std::optional<ParsedHostname> hostname =
      api::public_suffix::ParseHostname(gin::V8ToString(isolate, arguments[0]));
  if (!hostname.has_value()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = api::public_suffix::kInvalidHostname;
    return result;
  }

  RequestResult result(RequestResult::HANDLED);
  result.return_value =
      v8::Boolean::New(isolate, api::public_suffix::IsKnownSuffix(*hostname));
  return result;
}

RequestResult PublicSuffixHooksDelegate::HandleGetKnownSuffix(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  DCHECK(arguments[0]->IsString());

  v8::Isolate* isolate = script_context->isolate();
  std::optional<ParsedHostname> hostname =
      api::public_suffix::ParseHostname(gin::V8ToString(isolate, arguments[0]));
  if (!hostname.has_value()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = api::public_suffix::kInvalidHostname;
    return result;
  }

  RequestResult result(RequestResult::HANDLED);
  result.return_value = OptionalStringToV8(
      isolate, api::public_suffix::GetKnownSuffix(*hostname));
  return result;
}

RequestResult PublicSuffixHooksDelegate::HandleGetDomain(
    ScriptContext* script_context,
    const APISignature::V8ParseResult& parse_result) {
  const v8::LocalVector<v8::Value>& arguments = *parse_result.arguments;
  DCHECK_EQ(binding::AsyncResponseType::kNone, parse_result.async_type);
  DCHECK(arguments[0]->IsString());

  v8::Isolate* isolate = script_context->isolate();
  std::optional<ParsedHostname> hostname =
      api::public_suffix::ParseHostname(gin::V8ToString(isolate, arguments[0]));
  if (!hostname.has_value()) {
    RequestResult result(RequestResult::INVALID_INVOCATION);
    result.error = api::public_suffix::kInvalidHostname;
    return result;
  }

  DomainOptions options = ParseDomainOptions(isolate, arguments[1]);
  RequestResult result(RequestResult::HANDLED);
  result.return_value = OptionalStringToV8(
      isolate, api::public_suffix::GetDomain(*hostname, options));
  return result;
}

}  // namespace extensions
