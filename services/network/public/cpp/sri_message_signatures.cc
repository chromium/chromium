// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/url_util.h"
#include "net/http/structured_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"

namespace network {

namespace {

using ComponentParameter = mojom::SRIMessageSignatureComponentParameter;
using ComponentParameterPtr = mojom::SRIMessageSignatureComponentParameterPtr;
using ParameterType = mojom::SRIMessageSignatureComponentParameter::Type;

const size_t kEd25519KeyLength = 32;
const size_t kEd25519SigLength = 64;
constexpr std::string_view kAcceptSignature = "accept-signature";

constexpr std::array<std::string_view, 9u> kDerivedComponents = {
    "@query-param", "@query", "@path", "@status"
    // TODO(383409584): We should support the remaining derived components from
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-derived-components:
    //
    // "@authority",      "@method", "@query-param", "@query",
    // "@request-target", "@scheme", "@status",      "@target-uri",
};

ParameterType ParamNameToType(std::string_view name) {
  if (name == "name") {
    return ParameterType::kName;
  }
  if (name == "req") {
    return ParameterType::kRequest;
  }
  if (name == "sf") {
    return ParameterType::kStrictStructuredFieldSerialization;
  }
  NOTREACHED();
}

bool ItemHasBooleanParam(const net::structured_headers::ParameterizedItem& item,
                         std::string_view name) {
  for (const auto& param : item.params) {
    if (param.first == name && param.second.is_boolean() &&
        param.second.GetBoolean()) {
      return true;
    }
  }
  return false;
}

bool ItemHasStringParam(const net::structured_headers::ParameterizedItem& item,
                        std::string_view name) {
  for (const auto& param : item.params) {
    if (param.first == name && param.second.is_string()) {
      return true;
    }
  }
  return false;
}

std::optional<mojom::SRIMessageSignatureComponentPtr> ParseComponent(
    const net::structured_headers::ParameterizedItem& component,
    std::vector<mojom::SRIMessageSignatureError>& errors) {
  // https://wicg.github.io/signature-based-sri/#profile
  if (!component.item.is_string()) {
    errors.push_back(mojom::SRIMessageSignatureError::
                         kSignatureInputHeaderInvalidComponentType);
    return std::nullopt;
  }

  std::string name = component.item.GetString();
  auto result = mojom::SRIMessageSignatureComponent::New();
  result->name = name;

  // The "unencoded-digest" component requires a single `sf` parameter with
  // a `true` boolean value.
  if (name == "unencoded-digest") {
    if (!ItemHasBooleanParam(component, "sf") ||
        component.params.size() != 1u) {
      errors.push_back(
          mojom::SRIMessageSignatureError::
              kSignatureInputHeaderInvalidHeaderComponentParameter);
      return std::nullopt;
    }
    result->params.push_back(ComponentParameter::New(
        ParameterType::kStrictStructuredFieldSerialization, std::nullopt));
    return result;
  } else if (base::Contains(kDerivedComponents, name)) {
    // The `@status` derived component must not have any parameters (as it's
    // pulled from the response, not the request).
    if (name == "@status") {
      if (!component.params.empty()) {
        errors.push_back(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderInvalidDerivedComponentParameter);
        return std::nullopt;
      }
      return result;
    }

    // The `@query-param` derived component must have only a `name` parameter
    // with a string value, and a `req` parameter.
    if (name == "@query-param") {
      std::string name_value;
      if (!ItemHasStringParam(component, "name") ||
          !ItemHasBooleanParam(component, "req") ||
          component.params.size() != 2u) {
        errors.push_back(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderInvalidDerivedComponentParameter);
        return std::nullopt;
      }
      for (const auto& param : component.params) {
        std::optional<std::string> value;
        if (param.second.is_string()) {
          value = param.second.GetString();
        }
        result->params.push_back(
            ComponentParameter::New(ParamNameToType(param.first), value));
      }
      return result;
    }

    // All other derived components we've implemented require a single `req`
    // parameter with a `true` boolean value.
    if (!ItemHasBooleanParam(component, "req") ||
        component.params.size() != 1u) {
      errors.push_back(
          mojom::SRIMessageSignatureError::
              kSignatureInputHeaderInvalidDerivedComponentParameter);
      return std::nullopt;
    }
    result->params.push_back(
        ComponentParameter::New(ParameterType::kRequest, std::nullopt));
    return result;
  } else {
    errors.push_back(mojom::SRIMessageSignatureError::
                         kSignatureInputHeaderInvalidComponentName);
    return std::nullopt;
  }
}

// net::StructuredHeaders doesn't expose the ability to serialize a parameter
// list outside the context of a parameterized item. So, we'll do it ourselves
// by serializing each individually as an Item.
std::string SerializeParams(const net::structured_headers::Parameters params) {
  std::stringstream param_list;
  for (const auto& param : params) {
    const std::string& name = param.first;
    const net::structured_headers::Item& value = param.second;
    param_list << ';';

    // We only care about three parameter types for this specific application:
    //
    // 1.  Boolean for `sf` and `req` (which must be `true`).
    // 2.  Integers for `created` and `expires`.
    // 3.  String for everything else.
    DCHECK((value.is_boolean() && value.GetBoolean()) || value.is_integer() ||
           value.is_string());
    param_list << name;

    // For boolean parameters, we're done (as they wouldn't be in the list if
    // they weren't true, and we don't serialize `?1` for parameters. For other
    // types, we'll serialize the value:
    if (!value.is_boolean()) {
      std::optional<std::string> serialized_item =
          net::structured_headers::SerializeItem(value);
      DCHECK(serialized_item.has_value());
      param_list << '=' << serialized_item.value();
    }
  }
  return param_list.str();
}

std::string SerializeComponentParams(
    const std::vector<ComponentParameterPtr>& params) {
  std::stringstream param_list;
  for (const ComponentParameterPtr& param : params) {
    param_list << ';';
    switch (param->type) {
      case ParameterType::kName:
        DCHECK(param->value.has_value());
        param_list << "name=\"" << *param->value << "\"";
        break;
      case ParameterType::kRequest:
        param_list << "req";
        break;
      case ParameterType::kStrictStructuredFieldSerialization:
        param_list << "sf";
        break;
    }
  }
  return param_list.str();
}

// net::StructuredHeaders gives us the ability to serialize a list, but not an
// inner list. This is generally pretty reasonable, but unfortunately not what
// Section 2.3 of RFC9421 specifies for signature base serialization:
//
// https://www.rfc-editor.org/rfc/rfc9421#section-2.3
std::string SerializeInnerList(
    const std::vector<net::structured_headers::ParameterizedItem> list) {
  std::stringstream inner_list;
  // 1. Let the output be an empty string.
  // 2. Determine an order for the component identifiers of the covered
  //    components.
  //
  //    (We'll use the ordering as delivered in the header.)
  //
  // 3. Serialize the component identifiers ... as an ordered Inner List of
  //    String values ... append this to the output.
  inner_list << '(';
  for (const auto& component : list) {
    DCHECK(component.item.is_string());
    inner_list << '"' << component.item.GetString() << '"';
    inner_list << SerializeParams(component.params);

    // Put a space between each component, avoiding an extra space at the end.
    if (&component != &list.back()) {
      inner_list << ' ';
    }
  }
  inner_list << ')';
  return inner_list.str();
}

// Serialize the value of a single key from a `Signature-Input` header's
// Dictionary, as defined in Step 3 of Section 2.5 of RFC9421
// (https://www.rfc-editor.org/rfc/rfc9421#section-2.5).
std::string SerializeSignatureParams(
    const net::structured_headers::ParameterizedMember& input) {
  std::stringstream signature_params;

  // 3.   Append the signature parameters component (Section 2.3) ...
  // 3.1. Append the ... exact value `"@signature-params"`.
  // 3.2. Append a single colon (`:`).
  // 3.3. Append a single space (` `).
  signature_params << "\"@signature-params\": ";

  // 3.4. Append the signature parameters' canonicalized component values as
  //      defined in Section 2.3.
  DCHECK(input.member_is_inner_list);
  signature_params << SerializeInnerList(input.member);

  // 4. Determine an order for any signature parameters.
  //
  //    (We'll use the order in which they were delivered.)
  //
  // 5. Append the parameters to the inner list in order ... skipping
  //    parameters that are not available or not used for this message
  //    signature.
  signature_params << SerializeParams(input.params);

  return signature_params.str();
}

std::string SerializeDerivedComponent(
    const GURL& request_url,
    const int response_status_code,
    const mojom::SRIMessageSignatureComponentPtr& component) {
  DCHECK(base::Contains(kDerivedComponents, component->name));

  if (component->name == "@query") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-query
    return base::StrCat({"?", request_url.query()});
  } else if (component->name == "@query-param") {
    DCHECK(component->params.size() == 2u);
    auto name_it =
        std::find_if(component->params.begin(), component->params.end(),
                     [](const ComponentParameterPtr& p) {
                       return p->type == ParameterType::kName;
                     });
    DCHECK(name_it != component->params.end() && (*name_it)->value.has_value());
    std::string param_value;
    if (net::GetValueForKeyInQuery(request_url, *(*name_it)->value,
                                   &param_value)) {
      return base::EscapeAllExceptUnreserved(param_value);
    }
    return std::string();
  } else if (component->name == "@path") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#content-request-path
    return request_url.path();
  } else if (component->name == "@status") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#content-status-code
    return base::NumberToString(response_status_code);
  }

  // TODO(383409584): Support additional derived components.
  NOTREACHED();
}

//
// Validation during parsing.
//
// The functions in this section generally take a set of data to be validated as
// we parse through the `Signature` and `Signature-Input` headers, along with
// the vector of parsing errors we're tracking. If the data passes validation,
// the function will return true, and parsing should continue. If the data
// doesn't pass validation, the function will return `false`, and a relevant
// entry will be added to the list of parsing errors.
//
bool ValidateHeaderPresence(
    const std::string& signature_header,
    const std::string& signature_input_header,
    std::vector<mojom::SRIMessageSignatureError>& errors) {
  if (signature_header.empty() && signature_input_header.empty()) {
    // Neither `Signature` nor `Signature-Input` is present, punt on validation
    // without any errors.
    return false;
  } else if (signature_header.empty() && !signature_input_header.empty()) {
    errors.emplace_back(
        mojom::SRIMessageSignatureError::kMissingSignatureHeader);
    return false;
  } else if (signature_input_header.empty() && !signature_header.empty()) {
    errors.emplace_back(
        mojom::SRIMessageSignatureError::kMissingSignatureInputHeader);
    return false;
  }
  return true;
}

bool ValidateDictionaryStructure(
    std::optional<net::structured_headers::Dictionary> signature_dictionary,
    std::optional<net::structured_headers::Dictionary> input_dictionary,
    std::vector<mojom::SRIMessageSignatureError>& errors) {
  if (!signature_dictionary) {
    errors.emplace_back(
        mojom::SRIMessageSignatureError::kInvalidSignatureHeader);
    return false;
  }
  if (!input_dictionary) {
    errors.emplace_back(
        mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader);
    return false;
  }
  return true;
}

bool ValidateSignatureValue(
    const net::structured_headers::DictionaryMember& signature_entry,
    std::vector<mojom::SRIMessageSignatureError>& errors) {
  // The value must be an unparameterized byte-sequence:
  if (signature_entry.second.member.empty() ||
      signature_entry.second.member_is_inner_list ||
      !signature_entry.second.member[0].item.is_byte_sequence()) {
    errors.emplace_back(mojom::SRIMessageSignatureError::
                            kSignatureHeaderValueIsNotByteSequence);
    return false;
  } else if (signature_entry.second.params.size() != 0u) {
    errors.emplace_back(
        mojom::SRIMessageSignatureError::kSignatureHeaderValueIsParameterized);
    return false;
  }

  std::string signature = signature_entry.second.member[0].item.GetString();
  if (signature.size() != kEd25519SigLength) {
    errors.emplace_back(mojom::SRIMessageSignatureError::
                            kSignatureHeaderValueIsIncorrectLength);
    return false;
  }
  return true;
}

}  // namespace

mojom::SRIMessageSignaturesPtr ParseSRIMessageSignaturesFromHeaders(
    const net::HttpResponseHeaders& headers) {
  auto parsed_headers = mojom::SRIMessageSignatures::New();

  std::string signature_header =
      headers.GetNormalizedHeader("Signature").value_or("");
  std::string signature_input_header =
      headers.GetNormalizedHeader("Signature-Input").value_or("");
  if (!ValidateHeaderPresence(signature_header, signature_input_header,
                              parsed_headers->errors)) {
    return parsed_headers;
  }

  // Exit early if either the `Signature` or `Signature-Input` headers are
  // missing, or if they can't be parsed as structured field Dictionaries.
  std::optional<net::structured_headers::Dictionary> signature_dictionary =
      net::structured_headers::ParseDictionary(signature_header);
  std::optional<net::structured_headers::Dictionary> input_dictionary =
      net::structured_headers::ParseDictionary(signature_input_header);
  if (!ValidateDictionaryStructure(signature_dictionary, input_dictionary,
                                   parsed_headers->errors)) {
    return parsed_headers;
  }

  // Loop through the signature dictionary, matching each entry to its relevant
  // signature inputs as we go.
  //
  // Note that this means that we're accepting situations in which one header
  // has a superset of the entries in another header. The spec isn't exactly
  // clear on the expected client behavior here, suggesting that "The presence
  // of a label in one field but not the other is an error" but not providing
  // guidance on severity.
  //
  // https://datatracker.ietf.org/doc/html/rfc9421#section-4-4
  for (const net::structured_headers::DictionaryMember& signature_entry :
       signature_dictionary.value()) {
    auto message_signature = mojom::SRIMessageSignature::New();
    message_signature->label = signature_entry.first;

    if (!ValidateSignatureValue(signature_entry, parsed_headers->errors)) {
      continue;
    }
    std::string signature = signature_entry.second.member[0].item.GetString();
    message_signature->signature =
        std::vector<uint8_t>(signature.begin(), signature.end());

    // Grab the relevant `Signature-Input` entry, punting early if none exists
    // or if its value is not a non-empty parameterized inner-list.
    if (!input_dictionary->contains(signature_entry.first)) {
      parsed_headers->errors.push_back(
          mojom::SRIMessageSignatureError::kSignatureInputHeaderMissingLabel);
      continue;
    }
    auto input_entry = input_dictionary->at(signature_entry.first);
    if (!input_entry.member_is_inner_list) {
      parsed_headers->errors.push_back(
          mojom::SRIMessageSignatureError::
              kSignatureInputHeaderValueNotInnerList);
      continue;
    }

    for (const auto& component : input_entry.member) {
      // If any declared component is invalid, skip the signature (but not the
      // entire header; if both valid and invalid signatures are delivered,
      // we'll retain the former while ignoring the latter).
      std::optional<mojom::SRIMessageSignatureComponentPtr> parsed_component =
          ParseComponent(component, parsed_headers->errors);
      if (!parsed_component.has_value()) {
        message_signature.reset();
        break;
      }
      message_signature->components.push_back(
          std::move(parsed_component.value()));
    }

    if (!message_signature || message_signature->components.empty()) {
      parsed_headers->errors.push_back(
          mojom::SRIMessageSignatureError::
              kSignatureInputHeaderValueMissingComponents);
      continue;
    }

    // Process the parameters, according to the validation requirements at
    // https://wicg.github.io/signature-based-sri/#profile
    for (const auto& param : input_entry.params) {
      if (param.first == "created" && param.second.is_integer() &&
          param.second.GetInteger() >= 0) {
        message_signature->created = param.second.GetInteger();
      } else if (param.first == "expires" && param.second.is_integer() &&
                 param.second.GetInteger() >= 0) {
        message_signature->expires = param.second.GetInteger();
      } else if (param.first == "keyid" && param.second.is_string()) {
        std::string value = param.second.GetString();
        std::optional<std::vector<uint8_t>> decoded = base::Base64Decode(value);
        if (!decoded || decoded->size() != kEd25519KeyLength) {
          parsed_headers->errors.push_back(
              mojom::SRIMessageSignatureError::
                  kSignatureInputHeaderKeyIdLength);
          message_signature.reset();
          break;
        }
        message_signature->keyid = value;
      } else if (param.first == "nonce" && param.second.is_string()) {
        message_signature->nonce = param.second.GetString();
      } else if (param.first == "tag" && param.second.is_string() &&
                 param.second.GetString() == "sri") {
        message_signature->tag = "sri";
      } else {
        // The `alg` parameter must not be included in the signature input. Any
        // other parameters that aren't defined in the registry also
        // invalidate the signature.
        //
        // https://www.iana.org/assignments/http-message-signature/http-message-signature.xhtml#signature-metadata-parameters
        parsed_headers->errors.push_back(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderInvalidParameter);
        message_signature.reset();
        break;
      }
    }

    if (message_signature) {
      // Check required fields, and punt the signature if any are missing.
      if (!message_signature->keyid || !message_signature->tag) {
        parsed_headers->errors.push_back(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderMissingRequiredParameters);
        continue;
      }

      // Serialize `input_entry` as an inner list for later use in the signature
      // base. We're doing this work at parse time, as we can simply serialize
      // the structured field's parameterized member value that we processed
      // above, rather than storing the ordering of the parameters for
      // serialization later.
      message_signature->serialized_signature_params =
          SerializeSignatureParams(input_entry);

      // Otherwise, we're good! Save the signature and move on.
      parsed_headers->signatures.push_back(std::move(message_signature));
    }
  }

  return parsed_headers;
}

std::optional<std::string> ConstructSignatureBase(
    const mojom::SRIMessageSignaturePtr& signature,
    const GURL& request_url,
    const net::HttpResponseHeaders& headers) {
  if (!signature || !request_url.is_valid()) {
    return std::nullopt;
  }

  // Build the signature base per
  // https://www.rfc-editor.org/rfc/rfc9421.html#name-creating-the-signature-base
  std::stringstream signature_base;

  // 2. For each message component item in the covered components set (in
  //    order):
  for (const auto& component : signature->components) {
    // 2.1. If the component identifier (including its parameters) has already
    //      been added to the signature base, produce an error.
    //
    //      (We handle this at parse time)
    //
    // 2.2. Append the component identifier for the covered component ...
    signature_base << '"' << component->name << '"';
    signature_base << SerializeComponentParams(component->params);

    // 2.3. Append a single colon (`:`).
    // 2.4. Append a single space (` `).
    signature_base << ": ";

    // 2.5. Determine the component value for the component identifier.
    //
    //      (The error conditions listed in the spec for this step do not
    //       apply to the SRI-valid subset of message signatures.)
    //
    //      *  If the component name starts with an "at" (@) character, derive
    //         the component's value from the message according to the specific
    //         rules defined for the derived component, as provided in Section
    //         2.2, including processing of any known valid parameters. If the
    //         derived component name is unknown or the value cannot be derived,
    //         produce an error.
    std::optional<std::string> component_value;
    if (component->name.starts_with('@')) {
      if (!base::Contains(kDerivedComponents, component->name)) {
        return std::nullopt;
      }
      component_value = SerializeDerivedComponent(
          request_url, headers.response_code(), component);

      //      *  If the component name does not start with an "at" (`@`)
      //         character, canonizalize the HTTP field value ... If the field
      //         cannot be found in the message or the value cannot be obtained
      //         in the context, produce an error.
    } else {
      std::optional<std::string> header =
          headers.GetNormalizedHeader(component->name);
      if (!header.has_value()) {
        return std::nullopt;
      }

      // Determine how to serialize the header:
      //
      // SRI requires the `sf` parameter, which forces strict serialization for
      // structured fields.
      if (component->params.size() != 1u ||
          component->params[0]->type !=
              ParameterType::kStrictStructuredFieldSerialization) {
        return std::nullopt;
      }

      // Unfortunately, there doesn't seem to be a good way to decide how a
      // given structured field should be serialized (as a Dictionary? List?),
      // other than encoding a list of known headers and their types.
      // Fortunately, we only support one header at the moment, so the list is
      // manageable.
      if (component->name == "unencoded-digest") {
        std::optional<net::structured_headers::Dictionary> dict =
            net::structured_headers::ParseDictionary(header.value());
        if (!dict.has_value()) {
          return std::nullopt;
        }
        component_value =
            net::structured_headers::SerializeDictionary(dict.value());
      } else {
        return std::nullopt;
      }
    }
    // 2.6. Append the covered component's canonicalized component value.
    // 2.7. Append a single newline (`\n`).
    if (!component_value.has_value()) {
      return std::nullopt;
    }
    signature_base << component_value.value() << '\n';
  }

  // 3.   Append the signature parameters component:
  signature_base << signature->serialized_signature_params;

  // 4.   Produce an error if the output string contains non-ASCII characters.
  //      (This shouldn't be possible given the parsing rules for this profile.)
  std::string result = signature_base.str();
  DCHECK(base::IsStringASCII(result));

  // 5.   Return the output string.
  return result;
}

bool ValidateSRIMessageSignaturesOverHeaders(
    mojom::SRIMessageSignaturesPtr& message_signatures,
    const GURL& request_url,
    const net::HttpResponseHeaders& headers) {
  // If no signatures are present, validation automatically succeeds.
  if (!message_signatures->signatures.size() || !request_url.is_valid()) {
    return true;
  }

  // Loop through the signatures, validating each. Validation fails if any
  // given signature fails to validate.
  for (const auto& message_signature : message_signatures->signatures) {
    // Ensure the signature hasn't expired.
    if (message_signature->expires.has_value() &&
        message_signature->expires.value() <
            base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000) {
      message_signatures->errors.push_back(
          mojom::SRIMessageSignatureError::kValidationFailedSignatureExpired);
      return false;
    }

    // Generate the signature base:
    std::optional<std::string> signature_base =
        ConstructSignatureBase(message_signature, request_url, headers)
            .value_or("");

    // Decode the public key, and validate that both the public key and the
    // message's signature are the correct length for Ed25519 (32 and 64 bits,
    // respectively).
    std::string encoded_key = message_signature->keyid.value_or("");
    std::vector<uint8_t> public_key =
        base::Base64Decode(encoded_key).value_or(std::vector<uint8_t>{});
    if (public_key.size() != kEd25519KeyLength ||
        message_signature->signature.size() != kEd25519SigLength) {
      message_signatures->errors.push_back(
          mojom::SRIMessageSignatureError::kValidationFailedInvalidLength);
      return false;
    }

    // Verify the key and the signature over the signature base:
    if (!ED25519_verify(
            reinterpret_cast<const uint8_t*>(signature_base->data()),
            signature_base->size(), message_signature->signature.data(),
            public_key.data())) {
      message_signatures->errors.push_back(
          mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch);
      return false;
    }
  }

  return true;
}

std::optional<mojom::BlockedByResponseReason>
MaybeBlockResponseForSRIMessageSignature(
    const GURL& request_url,
    const network::mojom::URLResponseHead& response,
    bool checks_forced_by_initiator,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id) {
  // If the feature is disabled, never block resources.
  if (!base::FeatureList::IsEnabled(
          features::kSRIMessageSignatureEnforcement) &&
      !checks_forced_by_initiator) {
    return std::nullopt;
  }

  // No headers, no blocking.
  if (!response.headers || !request_url.is_valid()) {
    return std::nullopt;
  }
  auto parsed_headers = ParseSRIMessageSignaturesFromHeaders(*response.headers);
  bool passed_validation = !parsed_headers->signatures.size() ||
                           ValidateSRIMessageSignaturesOverHeaders(
                               parsed_headers, request_url, *response.headers);

  if (devtools_observer && !devtools_request_id.empty()) {
    for (const auto& error : parsed_headers->errors) {
      devtools_observer->OnSRIMessageSignatureError(devtools_request_id,
                                                    request_url, error);
    }
  }

  if (passed_validation) {
    return std::nullopt;
  }
  return mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch;
}

void MaybeSetAcceptSignatureHeader(
    net::URLRequest* request,
    const std::vector<std::string>& expected_signatures) {
  // In order to support request-specific experimentation, we send the
  // `Accept-Signature` header whenever signatures are expected by a request's
  // initiator, regardless of the `features::kSRIMessageSignatureEnforcement`
  // flag state.
  //
  // TODO(393924693): Remove this comment once we no longer need the origin
  // trial infrastructure.

  std::stringstream header;
  int counter = 0;
  for (const std::string& public_key : expected_signatures) {
    // We expect these to be validly base64-encoded Ed25519 public keys:
    std::optional<std::vector<uint8_t>> decoded =
        base::Base64Decode(public_key);
    if (!decoded || decoded->size() != kEd25519KeyLength) {
      continue;
    }

    // Build an `Accept-Signature` header, as a serialized Structured Field
    // dictionary, as per
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-the-accept-signature-field
    if (counter) {
      header << ", ";
    }
    header << "sig" << counter << "=(\"unencoded-digest\";sf);keyid=\""
           << public_key << "\";tag=\"sri\"";
    ++counter;
  }
  if (header.str().empty()) {
    return;
  }
  request->SetExtraRequestHeaderByName(kAcceptSignature, header.str(),
                                       /*overwrite=*/true);
}

}  // namespace network
