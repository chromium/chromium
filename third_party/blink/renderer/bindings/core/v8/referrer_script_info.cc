// Copyright 2017 The Chromium Authors
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

// Omit storing base URL if it is same as ScriptOrigin::ResourceName().
// Note: This improves chance of getting into a fast path in
//       ReferrerScriptInfo::ToV8HostDefinedOptions.
KURL GetStoredBaseUrl(const ReferrerScriptInfo& referrer_info,
                      const KURL& script_origin_resource_name) {
  if (referrer_info.BaseURL() == script_origin_resource_name)
    return KURL();

  // TODO(https://crbug.com/1235202): Currently when either `base_url_` is
  // `script_origin_resource_name` or null URL, they both result in
  // `script_origin_resource_name` in FromV8HostDefinedOptions(). Subsequent
  // CLs will fix this issue.
  if (referrer_info.BaseURL().IsNull())
    return KURL();

  return referrer_info.BaseURL();
}

ReferrerScriptInfo Default(const KURL& script_origin_resource_name) {
  // Default value. As base URL is null, defer to
  // `script_origin_resource_name`.
  ReferrerScriptInfo referrer_info(
      script_origin_resource_name, network::mojom::CredentialsMode::kSameOrigin,
      String(), kNotParserInserted, network::mojom::ReferrerPolicy::kDefault);
  DCHECK(referrer_info.IsDefaultValue(script_origin_resource_name));
  return referrer_info;
}

}  // namespace

bool ReferrerScriptInfo::IsDefaultValue(
    const KURL& script_origin_resource_name) const {
  return GetStoredBaseUrl(*this, script_origin_resource_name).IsNull() &&
         credentials_mode_ == network::mojom::CredentialsMode::kSameOrigin &&
         nonce_.empty() && parser_state_ == kNotParserInserted &&
         referrer_policy_ == network::mojom::ReferrerPolicy::kDefault;
}

ReferrerScriptInfo ReferrerScriptInfo::FromV8HostDefinedOptions(
    v8::Local<v8::Context> context,
    v8::Local<v8::Data> raw_host_defined_options,
    const KURL& script_origin_resource_name) {
  if (raw_host_defined_options.IsEmpty() ||
      !raw_host_defined_options->IsFixedArray()) {
    return Default(script_origin_resource_name);
  }
  v8::Local<v8::PrimitiveArray> host_defined_options =
      v8::Local<v8::PrimitiveArray>::Cast(raw_host_defined_options);
  if (!host_defined_options->Length()) {
    return Default(script_origin_resource_name);
  }

  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Primitive> base_url_value =
      host_defined_options->Get(isolate, kBaseURL);
  SECURITY_CHECK(base_url_value->IsString());
  String base_url_string =
      ToCoreString(isolate, v8::Local<v8::String>::Cast(base_url_value));
  KURL base_url = base_url_string.empty() ? KURL() : KURL(base_url_string);
  DCHECK(base_url.IsNull() || base_url.IsValid());
  if (base_url.IsNull()) {
    // If base URL is null, defer to `script_origin_resource_name`.
    base_url = script_origin_resource_name;
  }

  v8::Local<v8::Primitive> credentials_mode_value =
      host_defined_options->Get(isolate, kCredentialsMode);
  SECURITY_CHECK(credentials_mode_value->IsUint32());
  auto credentials_mode = static_cast<network::mojom::CredentialsMode>(
      credentials_mode_value->IntegerValue(context).ToChecked());

  v8::Local<v8::Primitive> nonce_value =
      host_defined_options->Get(isolate, kNonce);
  SECURITY_CHECK(nonce_value->IsString());
  String nonce =
      ToCoreString(isolate, v8::Local<v8::String>::Cast(nonce_value));

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

v8::Local<v8::Data> ReferrerScriptInfo::ToV8HostDefinedOptions(
    v8::Isolate* isolate,
    const KURL& script_origin_resource_name) const {
  if (IsDefaultValue(script_origin_resource_name))
    return v8::Local<v8::Data>();

  // TODO(cbruni, 1244145): Migrate to FixedArray or custom object.
  v8::Local<v8::PrimitiveArray> host_defined_options =
      v8::PrimitiveArray::New(isolate, HostDefinedOptionsIndex::kLength);

  const KURL stored_base_url =
      GetStoredBaseUrl(*this, script_origin_resource_name);

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
