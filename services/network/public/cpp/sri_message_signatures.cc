// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/features.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace network {

using Parameters = mojom::SRIMessageSignatureComponent::Parameter;

namespace {

const size_t kEd25519KeyLength = 32;
const size_t kEd25519SigLength = 64;

std::optional<mojom::SRIMessageSignatureComponentPtr> ParseComponent(
    const net::structured_headers::ParameterizedItem& component) {
  // We're quite restrictive at the moment: the only component we'll accept is
  // `identity-digest`, and we require that it has one and only parameter: `sf`.
  // Eventually, we'll support a broader set of headers and derived components,
  // but not today.
  //
  // https://wicg.github.io/signature-based-sri/#verification-requirements-for-sri
  if (!component.item.is_string()) {
    return std::nullopt;
  }

  std::string name = component.item.GetString();
  if (name == "identity-digest") {
    // The "identity-digest" component requires a single `sf` parameter with
    // a `true` boolean value.
    if (component.params.size() != 1u || component.params[0].first != "sf" ||
        !component.params[0].second.is_boolean() ||
        !component.params[0].second.GetBoolean()) {
      return std::nullopt;
    }
    auto result = mojom::SRIMessageSignatureComponent::New();
    result->name = name;
    result->params.push_back(Parameters::kStrictStructuredFieldSerialization);
    return result;
  } else {
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
    // 1.  Boolean for `sf` (which must be `true`).
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

}  // namespace

std::vector<mojom::SRIMessageSignaturePtr> ParseSRIMessageSignaturesFromHeaders(
    const net::HttpResponseHeaders& headers) {
  std::vector<mojom::SRIMessageSignaturePtr> parsed_headers;

  // Exit early if either the `Signature` or `Signature-Input` headers are
  // missing, or if they can't be parsed as structured field Dictionaries.
  std::optional<net::structured_headers::Dictionary> signature_dictionary =
      net::structured_headers::ParseDictionary(
          headers.GetNormalizedHeader("Signature").value_or(""));
  std::optional<net::structured_headers::Dictionary> input_dictionary =
      net::structured_headers::ParseDictionary(
          headers.GetNormalizedHeader("Signature-Input").value_or(""));
  if (!signature_dictionary || !input_dictionary) {
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

    // Skip entries whose values are anything other than unparameterized
    // byte-sequences:
    if (signature_entry.second.member_is_inner_list ||
        signature_entry.second.member.empty() ||
        !signature_entry.second.member[0].item.is_byte_sequence() ||
        signature_entry.second.member[0].params.size() != 0u) {
      continue;
    }

    std::string signature = signature_entry.second.member[0].item.GetString();
    message_signature->signature =
        std::vector<uint8_t>(signature.begin(), signature.end());
    if (message_signature->signature.size() != kEd25519SigLength) {
      continue;
    }

    // Grab the relevant `Signature-Input` entry, punting early if none exists
    // or if its value is not a non-empty parameterized inner-list.
    if (!input_dictionary->contains(signature_entry.first)) {
      continue;
    }
    auto input_entry = input_dictionary->at(signature_entry.first);
    if (!input_entry.member_is_inner_list) {
      continue;
    }

    for (const auto& component : input_entry.member) {
      // If any declared component is invalid, skip the signature (but not the
      // entire header; if both valid and invalid signatures are delivered,
      // we'll retain the former while ignoring the latter).
      std::optional<mojom::SRIMessageSignatureComponentPtr> parsed_component =
          ParseComponent(component);
      if (!parsed_component.has_value()) {
        message_signature.reset();
        break;
      }
      message_signature->components.push_back(
          std::move(parsed_component.value()));
    }

    if (!message_signature || message_signature->components.empty()) {
      continue;
    }

    // Process the parameters, according to the validation requirements at
    // https://wicg.github.io/signature-based-sri/#profile
    for (const auto& param : input_entry.params) {
      if (param.first == "alg" && param.second.is_string() &&
          param.second.GetString() == "ed25519") {
        message_signature->alg =
            mojom::SRIMessageSignature::Algorithm::kEd25519;
      } else if (param.first == "created" && param.second.is_integer() &&
                 param.second.GetInteger() >= 0) {
        message_signature->created = param.second.GetInteger();
      } else if (param.first == "expires" && param.second.is_integer() &&
                 param.second.GetInteger() >= 0) {
        message_signature->expires = param.second.GetInteger();
      } else if (param.first == "keyid" && param.second.is_string()) {
        std::string value = param.second.GetString();
        std::optional<std::vector<uint8_t>> decoded = base::Base64Decode(value);
        if (!decoded || decoded->size() != kEd25519KeyLength) {
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
        message_signature.reset();
        break;
      }
    }

    if (message_signature) {
      // Check required fields, and punt the signature if any are missing.
      if (!message_signature->alg || !message_signature->keyid ||
          !message_signature->tag) {
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
      parsed_headers.push_back(std::move(message_signature));
    }
  }

  return parsed_headers;
}

std::optional<std::string> ConstructSignatureBase(
    const mojom::SRIMessageSignaturePtr& signature,
    const net::HttpResponseHeaders& headers) {
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
    // 2.3. Append a single colon (`:`).
    // 2.4. Append a single space (` `).
    signature_base << '"' << component->name << "\": ";

    // 2.5. Determine the component value for the component identifier.
    //
    //      (The error conditions listed in the spec for this step do not
    //       apply to the SRI-valid subset of message signatures.)
    //
    //      *  If the component name does not start with an "at" (`@`)
    //         character, canonizalize the HTTP field value ... If the field
    //         cannot be found in the message or the value cannot be obtained
    //         in the context, produce an error.
    std::optional<std::string> component_value;
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
        component->params[0] !=
            Parameters::kStrictStructuredFieldSerialization) {
      return std::nullopt;
    }

    // Unfortunately, there doesn't seem to be a good way to decide how a
    // given structured field should be serialized (as a Dictionary? List?),
    // other than encoding a list of known headers and their types.
    // Fortunately, we only support one header at the moment, so the list is
    // managable.
    if (component->name == "identity-digest") {
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
    const std::vector<mojom::SRIMessageSignaturePtr>& message_signatures,
    const net::HttpResponseHeaders& headers) {
  // If no signatures are present, validation automatically succeeds.
  if (!message_signatures.size()) {
    return true;
  }

  // Loop through the signatures, validating each. Validation fails if any
  // given signature fails to validate.
  for (const auto& message_signature : message_signatures) {
    // Ensure the signature hasn't expired.
    if (message_signature->expires.has_value() &&
        message_signature->expires.value() <
            base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000) {
      return false;
    }

    // Generate the signature base:
    std::optional<std::string> signature_base =
        ConstructSignatureBase(message_signature, headers).value_or("");

    // Decode the public key, and validate that both the public key and the
    // message's signature are the correct length for Ed25519 (32 and 64 bits,
    // respectively).
    std::string encoded_key = message_signature->keyid.value_or("");
    std::vector<uint8_t> public_key =
        base::Base64Decode(encoded_key).value_or(std::vector<uint8_t>{});
    if (public_key.size() != kEd25519KeyLength ||
        message_signature->signature.size() != kEd25519SigLength) {
      return false;
    }

    // Verify the key and the signature over the signature base:
    if (!ED25519_verify(
            reinterpret_cast<const uint8_t*>(signature_base->data()),
            signature_base->size(), message_signature->signature.data(),
            public_key.data())) {
      return false;
    }
  }

  return true;
}

std::optional<mojom::BlockedByResponseReason>
MaybeBlockResponseForSRIMessageSignature(
    const network::mojom::URLResponseHead& response) {
  // If the feature is disabled, never block resources.
  if (!base::FeatureList::IsEnabled(
          features::kSRIMessageSignatureEnforcement)) {
    return std::nullopt;
  }

  // No headers, no blocking.
  if (!response.headers) {
    return std::nullopt;
  }
  auto signatures = ParseSRIMessageSignaturesFromHeaders(*response.headers);
  if (!signatures.size() ||
      ValidateSRIMessageSignaturesOverHeaders(signatures, *response.headers)) {
    return std::nullopt;
  }
  return mojom::BlockedByResponseReason::kSRIMessageSignatureMismatch;
}

}  // namespace network
