// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"

#include "mojo/public/cpp/bindings/enum_utils.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

enum HostDefinedOptionsIndex : size_t {
  kBaseURL,
  kCredentialsMode,
  kNonce,
  kParserState,
  kReferrerPolicy,
  kLength
};

}  // namespace

ReferrerScriptInfo ReferrerScriptInfo::FromV8HostDefinedOptions(
    v8::Local<v8::Context> context,
    v8::Local<v8::PrimitiveArray> host_defined_options) {
  if (host_defined_options.IsEmpty() || !host_defined_options->Length()) {
    return ReferrerScriptInfo();
  }

  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Primitive> base_url_value =
      host_defined_options->Get(isolate, kBaseURL);
  SECURITY_CHECK(base_url_value->IsString());
  String base_url_string =
      ToCoreString(v8::Local<v8::String>::Cast(base_url_value));
  KURL base_url = base_url_string.IsEmpty() ? KURL() : KURL(base_url_string);
  DCHECK(base_url.IsNull() || base_url.IsValid());

  v8::Local<v8::Primitive> credentials_mode_value =
      host_defined_options->Get(isolate, kCredentialsMode);
  SECURITY_CHECK(credentials_mode_value->IsUint32());
  auto credentials_mode = static_cast<network::mojom::CredentialsMode>(
      credentials_mode_value->IntegerValue(context).ToChecked());

  v8::Local<v8::Primitive> nonce_value =
      host_defined_options->Get(isolate, kNonce);
  SECURITY_CHECK(nonce_value->IsString());
  String nonce = ToCoreString(v8::Local<v8::String>::Cast(nonce_value));

  v8::Local<v8::Primitive> parser_state_value =
      host_defined_options->Get(isolate, kParserState);
  SECURITY_CHECK(parser_state_value->IsUint32());
  ParserDisposition parser_state = static_cast<ParserDisposition>(
      parser_state_value->IntegerValue(context).ToChecked());

  v8::Local<v8::Primitive> referrer_policy_value =
      host_defined_options->Get(isolate, kReferrerPolicy);
  SECURITY_CHECK(referrer_policy_value->IsUint32());
  int32_t referrer_policy_int32 = base::saturated_cast<int32_t>(
      referrer_policy_value->IntegerValue(context).ToChecked());
  network::mojom::ReferrerPolicy referrer_policy =
      mojo::ConvertIntToMojoEnum<network::mojom::ReferrerPolicy>(
          referrer_policy_int32)
          .value_or(network::mojom::ReferrerPolicy::kDefault);

  return ReferrerScriptInfo(base_url, credentials_mode, nonce, parser_state,
                            referrer_policy);
}

v8::Local<v8::PrimitiveArray> ReferrerScriptInfo::ToV8HostDefinedOptions(
    v8::Isolate* isolate) const {
  if (IsDefaultValue())
    return v8::Local<v8::PrimitiveArray>();

  v8::Local<v8::PrimitiveArray> host_defined_options =
      v8::PrimitiveArray::New(isolate, HostDefinedOptionsIndex::kLength);

  v8::Local<v8::Primitive> base_url_value =
      V8String(isolate, base_url_.GetString());
  host_defined_options->Set(isolate, HostDefinedOptionsIndex::kBaseURL,
                            base_url_value);

  v8::Local<v8::Primitive> credentials_mode_value =
      v8::Integer::NewFromUnsigned(isolate,
                                   static_cast<uint32_t>(credentials_mode_));
  host_defined_options->Set(isolate, HostDefinedOptionsIndex::kCredentialsMode,
                            credentials_mode_value);

  v8::Local<v8::Primitive> nonce_value = V8String(isolate, nonce_);
  host_defined_options->Set(isolate, HostDefinedOptionsIndex::kNonce,
                            nonce_value);

  v8::Local<v8::Primitive> parser_state_value = v8::Integer::NewFromUnsigned(
      isolate, static_cast<uint32_t>(parser_state_));
  host_defined_options->Set(isolate, HostDefinedOptionsIndex::kParserState,
                            parser_state_value);

  v8::Local<v8::Primitive> referrer_policy_value = v8::Integer::NewFromUnsigned(
      isolate, static_cast<uint32_t>(referrer_policy_));
  host_defined_options->Set(isolate, HostDefinedOptionsIndex::kReferrerPolicy,
                            referrer_policy_value);

  return host_defined_options;
}

}  // namespace blink
