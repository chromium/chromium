// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
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
    "@authority", "@query-param", "@query",  "@method",
    "@path",      "@scheme",      "@status", "@target-uri",
    // TODO(383409584): We should support the remaining derived components from
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-derived-components:
    //
    // "@request-target"
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
  if (name == "bs") {
    return ParameterType::kBinaryRepresentation;
  }
  NOTREACHED();
}

bool IsRequestComponent(
    const mojom::SRIMessageSignatureComponentPtr& component) {
  return std::ranges::any_of(component->params, [](const auto& p) {
    return p->type == ParameterType::kRequest;
  });
}

bool IsStrictlySerializedComponent(
    const mojom::SRIMessageSignatureComponentPtr& component) {
  return std::ranges::any_of(component->params, [](const auto& p) {
    return p->type == ParameterType::kStrictStructuredFieldSerialization;
  });
}

bool IsBinaryWrappedComponent(
    const mojom::SRIMessageSignatureComponentPtr& component) {
  return std::ranges::any_of(component->params, [](const auto& p) {
    return p->type == ParameterType::kBinaryRepresentation;
  });
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

void AddIssueFromErrorEnum(
    mojom::SRIMessageSignatureError error_code,
    std::vector<mojom::SRIMessageSignatureIssuePtr>& out) {
  auto issue = mojom::SRIMessageSignatureIssue::New();
  issue->error = error_code;
  out.push_back(std::move(issue));
}

std::optional<mojom::SRIMessageSignatureComponentPtr> ParseComponent(
    const net::structured_headers::ParameterizedItem& component,
    std::vector<mojom::SRIMessageSignatureIssuePtr>& issues) {
  // https://wicg.github.io/signature-based-sri/#profile
  if (!component.item.is_string()) {
    AddIssueFromErrorEnum(mojom::SRIMessageSignatureError::
                              kSignatureInputHeaderInvalidComponentType,
                          issues);
    return std::nullopt;
  }

  std::string name = component.item.GetString();
  auto result = mojom::SRIMessageSignatureComponent::New();
  result->name = name;

  if (name == "unencoded-digest") {
    // The "unencoded-digest" component requires a single `sf` parameter with
    // a `true` boolean value.
    if (!ItemHasBooleanParam(component, "sf") ||
        component.params.size() != 1u) {
      AddIssueFromErrorEnum(
          mojom::SRIMessageSignatureError::
              kSignatureInputHeaderInvalidHeaderComponentParameter,
          issues);
      return std::nullopt;
    }
    result->params.push_back(ComponentParameter::New(
        ParameterType::kStrictStructuredFieldSerialization, std::nullopt));
    return result;
  } else if (!name.starts_with('@') && net::HttpUtil::IsValidHeaderName(name) &&
             name == base::ToLowerASCII(name)) {
    // All other headers may specify the `req` and `bs` parameters.
    for (const auto& param : component.params) {
      if (param.second.is_boolean() && param.second.GetBoolean() &&
          (param.first == "req" || param.first == "bs")) {
        result->params.push_back(ComponentParameter::New(
            ParamNameToType(param.first), std::nullopt));
      } else {
        AddIssueFromErrorEnum(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderInvalidHeaderComponentParameter,
            issues);
        return std::nullopt;
      }
    }
    return result;
  } else if (base::Contains(kDerivedComponents, name)) {
    // The `@status` derived component must not have any parameters (as it's
    // pulled from the response, not the request).
    if (name == "@status") {
      if (!component.params.empty()) {
        AddIssueFromErrorEnum(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderInvalidDerivedComponentParameter,
            issues);
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
        AddIssueFromErrorEnum(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderInvalidDerivedComponentParameter,
            issues);
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
      AddIssueFromErrorEnum(
          mojom::SRIMessageSignatureError::
              kSignatureInputHeaderInvalidDerivedComponentParameter,
          issues);
      return std::nullopt;
    }
    result->params.push_back(
        ComponentParameter::New(ParameterType::kRequest, std::nullopt));
    return result;
  } else {
    AddIssueFromErrorEnum(mojom::SRIMessageSignatureError::
                              kSignatureInputHeaderInvalidComponentName,
                          issues);
    return std::nullopt;
  }
}

std::optional<std::string> SerializeByteSequence(std::string_view input) {
  return net::structured_headers::SerializeItem(net::structured_headers::Item(
      std::string(input), net::structured_headers::Item::kByteSequenceType));
}

// net::StructuredHeaders doesn't expose the ability to serialize a parameter
// list outside the context of a parameterized item. So, we'll do it ourselves
// by serializing each individually as an Item.
std::string SerializeParams(const net::structured_headers::Parameters params) {
  std::stringstream param_list;
  for (const auto& param : params) {
    const std::string& name = param.first;
    const net::structured_headers::Item& value = param.second;
    param_list << ';' << name;

    // For boolean parameters, we're done if the parameter's value is true (as
    // per https://www.rfc-editor.org/rfc/rfc9651#section-3.1.2-6). For any
    // other value or type, we'll serialize the value explicitly.
    if (value.is_boolean() && value.GetBoolean()) {
      continue;
    }
    std::optional<std::string> serialized_item =
        net::structured_headers::SerializeItem(value);
    DCHECK(serialized_item.has_value());
    param_list << '=' << serialized_item.value();
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
      case ParameterType::kBinaryRepresentation:
        param_list << "bs";
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
    const net::URLRequest& url_request,
    const int response_status_code,
    const mojom::SRIMessageSignatureComponentPtr& component) {
  DCHECK(base::Contains(kDerivedComponents, component->name));
  DCHECK(url_request.url().is_valid());

  if (component->name == "@authority") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-authority
    if (url_request.url().has_port()) {
      return base::StrCat(
          {url_request.url().host(), ":", url_request.url().port()});
    }
    return url_request.url().GetHost();
  } else if (component->name == "@query") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-query
    return base::StrCat({"?", url_request.url().GetQuery()});
  } else if (component->name == "@query-param") {
    DCHECK(component->params.size() == 2u);
    auto name_it =
        std::find_if(component->params.begin(), component->params.end(),
                     [](const ComponentParameterPtr& p) {
                       return p->type == ParameterType::kName;
                     });
    DCHECK(name_it != component->params.end() && (*name_it)->value.has_value());
    std::string param_value;
    if (net::GetValueForKeyInQuery(url_request.url(), *(*name_it)->value,
                                   &param_value)) {
      return base::EscapeAllExceptUnreserved(param_value);
    }
    return std::string();
  } else if (component->name == "@method") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#content-request-method
    return url_request.method();
  } else if (component->name == "@path") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#content-request-path
    return url_request.url().GetPath();
  } else if (component->name == "@scheme") {
    return url_request.url().GetScheme();
  } else if (component->name == "@status") {
    // https://www.rfc-editor.org/rfc/rfc9421.html#content-status-code
    return base::NumberToString(response_status_code);
  } else if (component->name == "@target-uri") {
    // While we certainly need to clear any fragment component present in the
    // requested URL, it's unclear whether `@target-uri` is intended to include
    // the `userinfo` portion of a requested URL. For the moment, we'll strip
    // those components as well, just as we do for referrers.
    //
    // https://datatracker.ietf.org/doc/html/rfc9421#content-target-uri
    return url_request.url().GetAsReferrer().spec();
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
    std::vector<mojom::SRIMessageSignatureIssuePtr>& issues) {
  if (signature_header.empty() && signature_input_header.empty()) {
    // Neither `Signature` nor `Signature-Input` is present, punt on validation
    // without any errors.
    return false;
  } else if (signature_header.empty() && !signature_input_header.empty()) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kMissingSignatureHeader, issues);
    return false;
  } else if (signature_input_header.empty() && !signature_header.empty()) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kMissingSignatureInputHeader, issues);
    return false;
  }
  return true;
}

bool ValidateDictionaryStructure(
    std::optional<net::structured_headers::Dictionary> signature_dictionary,
    std::optional<net::structured_headers::Dictionary> input_dictionary,
    std::vector<mojom::SRIMessageSignatureIssuePtr>& issues) {
  if (!signature_dictionary) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kInvalidSignatureHeader, issues);
    return false;
  }
  if (!input_dictionary) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kInvalidSignatureInputHeader, issues);
    return false;
  }
  return true;
}

bool ValidateSignatureValue(
    const net::structured_headers::DictionaryMember& signature_entry,
    std::vector<mojom::SRIMessageSignatureIssuePtr>& issues) {
  // The value must be an unparameterized byte-sequence:
  if (signature_entry.second.member.empty() ||
      signature_entry.second.member_is_inner_list ||
      !signature_entry.second.member[0].item.is_byte_sequence()) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kSignatureHeaderValueIsNotByteSequence,
        issues);
    return false;
  } else if (signature_entry.second.params.size() != 0u) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kSignatureHeaderValueIsParameterized,
        issues);
    return false;
  }

  std::string signature = signature_entry.second.member[0].item.GetString();
  if (signature.size() != kEd25519SigLength) {
    AddIssueFromErrorEnum(
        mojom::SRIMessageSignatureError::kSignatureHeaderValueIsIncorrectLength,
        issues);
    return false;
  }
  return true;
}

bool MatchExpectedPublicKeys(
    mojom::SRIMessageSignaturesPtr& message_signatures,
    const std::vector<std::vector<uint8_t>>& expected_public_keys) {
  if (expected_public_keys.empty()) {
    return true;
  }
  for (const auto& key : expected_public_keys) {
    for (const auto& signature : message_signatures->signatures) {
      if (signature->keyid && signature->keyid.value() == key) {
        return true;
      }
    }
  }

  // We failed to match above, so add an issue and return false:
  auto issue = mojom::SRIMessageSignatureIssue::New();
  issue->error =
      mojom::SRIMessageSignatureError::kValidationFailedIntegrityMismatch;
  issue->integrity_assertions.emplace();
  for (const auto& key : expected_public_keys) {
    issue->integrity_assertions->push_back(base::Base64Encode(key));
  }
  message_signatures->issues.push_back(std::move(issue));
  return false;
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
                              parsed_headers->issues)) {
    return parsed_headers;
  }

  // Exit early if either the `Signature` or `Signature-Input` headers are
  // missing, or if they can't be parsed as structured field Dictionaries.
  std::optional<net::structured_headers::Dictionary> signature_dictionary =
      net::structured_headers::ParseDictionary(signature_header);
  std::optional<net::structured_headers::Dictionary> input_dictionary =
      net::structured_headers::ParseDictionary(signature_input_header);
  if (!ValidateDictionaryStructure(signature_dictionary, input_dictionary,
                                   parsed_headers->issues)) {
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

    if (!ValidateSignatureValue(signature_entry, parsed_headers->issues)) {
      continue;
    }
    std::string signature = signature_entry.second.member[0].item.GetString();
    message_signature->signature =
        std::vector<uint8_t>(signature.begin(), signature.end());

    // Grab the relevant `Signature-Input` entry, punting early if none exists
    // or if its value is not a non-empty parameterized inner-list.
    if (!input_dictionary->contains(signature_entry.first)) {
      AddIssueFromErrorEnum(
          mojom::SRIMessageSignatureError::kSignatureInputHeaderMissingLabel,
          parsed_headers->issues);
      continue;
    }
    auto input_entry = input_dictionary->at(signature_entry.first);
    if (!input_entry.member_is_inner_list) {
      AddIssueFromErrorEnum(mojom::SRIMessageSignatureError::
                                kSignatureInputHeaderValueNotInnerList,
                            parsed_headers->issues);
      continue;
    }

    // Step 1.1 of the signature validation requirements punts on any signature
    // input whose `tag`parameter is not valid for SRI. We'll do that before
    // parsing the header's components or parameters, as it might be valid for
    // some other scheme, and any issues we'd record would be confusing in
    // that case. The preceding checks (does a `Signature` and
    // `Signature-Input` pair exist for a given name, and is the component set
    // an inner-list) should be valid for any RFC9421-based system, so we'll
    // perform those first.
    //
    // https://wicg.github.io/signature-based-sri/#abstract-opdef-validating-an-integrity-signature
    if (!std::ranges::any_of(input_entry.params, [](const auto& param) {
      return (param.first == "tag" && param.second.is_string() &&
          (param.second.GetString() == "ed25519-integrity" ||
           param.second.GetString() == "sri"));
    })) {
      continue;
    }

    // Process the components.
    for (const auto& component : input_entry.member) {
      // If any declared component is invalid, skip the signature (but not the
      // entire header; if both valid and invalid signatures are delivered,
      // we'll retain the former while ignoring the latter).
      std::optional<mojom::SRIMessageSignatureComponentPtr> parsed_component =
          ParseComponent(component, parsed_headers->issues);
      if (!parsed_component.has_value()) {
        message_signature.reset();
        break;
      }
      message_signature->components.push_back(
          std::move(parsed_component.value()));
    }

    // The signature's component list must include `unencoded-digest`.
    if (!message_signature || message_signature->components.empty() ||
        std::ranges::none_of(message_signature->components, [](const auto& c) {
          return c->name == "unencoded-digest";
        })) {
      AddIssueFromErrorEnum(mojom::SRIMessageSignatureError::
                                kSignatureInputHeaderValueMissingComponents,
                            parsed_headers->issues);
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
          AddIssueFromErrorEnum(
              mojom::SRIMessageSignatureError::kSignatureInputHeaderKeyIdLength,
              parsed_headers->issues);
          message_signature.reset();
          break;
        }
        message_signature->keyid = std::move(*decoded);
      } else if (param.first == "nonce" && param.second.is_string()) {
        message_signature->nonce = param.second.GetString();
      } else if (param.first == "tag" && param.second.is_string() &&
                 (param.second.GetString() == "ed25519-integrity" ||
                  param.second.GetString() == "sri")) {
        // TODO(crbug.com/419149647): Drop support for `sri` once tests are
        // updated and OT participants have adopted the new `tag`.
        message_signature->tag = param.second.GetString();
      } else if (param.first == "alg" || param.first == "created" ||
                 param.first == "expires" || param.first == "keyid" ||
                 param.first == "nonce" || param.first == "tag") {
        // The `alg` parameter must not be included in the signature input, and
        // we'll only reach this branch for other known parameter names if they
        // didn't meet the type constraints tested above. In either case, we'll
        // throw an error and reject this signature.
        //
        // https://wicg.github.io/signature-based-sri/#profile
        AddIssueFromErrorEnum(mojom::SRIMessageSignatureError::
                                  kSignatureInputHeaderInvalidParameter,
                              parsed_headers->issues);
        message_signature.reset();
        break;
      }
      // We do not otherwise act upon unknown signature parameters. They'll be
      // part of the serialized `@signature-params`, but will not have any
      // additional effect.
    }

    if (message_signature) {
      // Check required fields, and punt the signature if any are missing.
      if (!message_signature->keyid || !message_signature->tag) {
        AddIssueFromErrorEnum(
            mojom::SRIMessageSignatureError::
                kSignatureInputHeaderMissingRequiredParameters,
            parsed_headers->issues);
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
    const net::URLRequest& url_request,
    const net::HttpResponseHeaders& headers) {
  const GURL request_url = url_request.url();
  DCHECK(request_url.is_valid());

  if (!signature) {
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
          url_request, headers.response_code(), component);

      //      *  If the component name does not start with an "at" (`@`)
      //         character, canonizalize the HTTP field value ... If the field
      //         cannot be found in the message or the value cannot be obtained
      //         in the context, produce an error.
    } else {
      // Grab the header from the request or response as appropriate, punting
      // out of signature base generation if the header isn't present
      std::optional<std::string> header =
          IsRequestComponent(component)
              ? url_request.extra_request_headers().GetHeader(component->name)
              : headers.GetNormalizedHeader(component->name);
      if (!header.has_value()) {
        // TODO(mkwst): We should have a more-specific error here.
        return std::nullopt;
      }

      // Determine how to serialize the header:
      if (IsBinaryWrappedComponent(component)) {
        component_value = SerializeByteSequence(header.value());
      } else if (IsStrictlySerializedComponent(component)) {
        // Unfortunately, there doesn't seem to be a good way to decide how a
        // given structured field should be serialized (as a Dictionary? List?),
        // other than encoding a list of known headers and their types.
        // Fortunately, we only support one header at the moment, so the list is
        // manageable.
        if (component->name == "unencoded-digest") {
          // TODO(mkwst): We shouldn't parse this header both here and in Blink.
          // Ideally we'll migrate the implementation into the network stack.
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
      } else {
        component_value = header.value();
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
    const net::URLRequest& url_request,
    const net::HttpResponseHeaders& headers) {
  const GURL request_url = url_request.url();
  DCHECK(url_request.url().is_valid());

  // If no signatures are present, validation automatically succeeds.
  if (!message_signatures->signatures.size()) {
    return true;
  }

  // Loop through the signatures, validating each. Validation fails if any
  // given signature fails to validate.
  for (const auto& message_signature : message_signatures->signatures) {
    // Ensure the signature hasn't expired.
    if (message_signature->expires.has_value() &&
        message_signature->expires.value() <
            base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000) {
      AddIssueFromErrorEnum(
          mojom::SRIMessageSignatureError::kValidationFailedSignatureExpired,
          message_signatures->issues);
      return false;
    }

    // Generate the signature base:
    std::string signature_base =
        ConstructSignatureBase(message_signature, url_request, headers)
            .value_or("");

    // Decode the public key, and validate that both the public key and the
    // message's signature are the correct length for Ed25519 (32 and 64 bits,
    // respectively).
    const std::vector<uint8_t>& public_key =
        message_signature->keyid.value_or(std::vector<uint8_t>{});
    if (public_key.size() != kEd25519KeyLength ||
        message_signature->signature.size() != kEd25519SigLength) {
      AddIssueFromErrorEnum(
          mojom::SRIMessageSignatureError::kValidationFailedInvalidLength,
          message_signatures->issues);
      return false;
    }

    // Verify the key and the signature over the signature base:
    if (!ED25519_verify(reinterpret_cast<const uint8_t*>(signature_base.data()),
                        signature_base.size(),
                        message_signature->signature.data(),
                        public_key.data())) {
      auto issue = mojom::SRIMessageSignatureIssue::New();
      issue->error =
          mojom::SRIMessageSignatureError::kValidationFailedSignatureMismatch;
      issue->signature_base = signature_base;
      message_signatures->issues.push_back(std::move(issue));
      return false;
    }
  }

  return true;
}

std::optional<mojom::BlockedByResponseReason>
MaybeBlockResponseForSRIMessageSignature(
    const net::URLRequest& url_request,
    const network::mojom::URLResponseHead& response,
    const std::vector<std::vector<uint8_t>>& expected_public_keys,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id) {
  // No headers, no URL: no blocking.
  const GURL request_url = url_request.url();
  if (!response.headers || !request_url.is_valid()) {
    return std::nullopt;
  }
  auto parsed_headers = ParseSRIMessageSignaturesFromHeaders(*response.headers);
  bool passed_validation =
      !parsed_headers->signatures.size() ||
      (ValidateSRIMessageSignaturesOverHeaders(parsed_headers, url_request,
                                               *response.headers) &&
       MatchExpectedPublicKeys(parsed_headers, expected_public_keys));

  if (devtools_observer && !devtools_request_id.empty()) {
    devtools_observer->OnSRIMessageSignatureIssue(
        devtools_request_id, request_url, std::move(parsed_headers->issues));
  }

  if (passed_validation) {
    return std::nullopt;
  }
  return mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch;
}

void MaybeSetAcceptSignatureHeader(
    net::URLRequest* request,
    const std::vector<std::vector<uint8_t>>& expected_public_keys) {
  std::stringstream header;
  int counter = 0;
  for (const auto& public_key : expected_public_keys) {
    // We expect these to be valid lengths for Ed25519 public keys:
    if (public_key.size() != kEd25519KeyLength) {
      continue;
    }

    // Build an `Accept-Signature` header, as a serialized Structured Field
    // dictionary, as per
    // https://www.rfc-editor.org/rfc/rfc9421.html#name-the-accept-signature-field
    if (counter) {
      header << ", ";
    }
    header << "sig" << counter << "=(\"unencoded-digest\";sf);keyid=\""
           << base::Base64Encode(public_key) << "\";tag=\"ed25519-integrity\"";
    ++counter;
  }
  if (header.str().empty()) {
    return;
  }
  request->SetExtraRequestHeaderByName(kAcceptSignature, header.str(),
                                       /*overwrite=*/true);
}

}  // namespace network
