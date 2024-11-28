// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/sri_message_signatures.h"

#include "base/base64.h"
#include "net/http/structured_headers.h"

namespace network {

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
    result->params.push_back(mojom::SRIMessageSignatureComponent::Parameter::
                                 kStrictStructuredFieldSerialization);
    return result;
  } else {
    return std::nullopt;
  }
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
    // https://wicg.github.io/signature-based-sri/#validation
    std::string previous_param;
    for (const auto& param : input_entry.params) {
      // Verify that parameters were specified in alphabetical order:
      if (param.first < previous_param) {
        message_signature.reset();
        break;
      } else {
        previous_param = param.first;
      }

      if (param.first == "alg" && param.second.is_string() &&
          param.second.GetString() == "ed25519") {
        message_signature->alg = "ed25519";
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

      // Otherwise, we're good! Save the signature and move on.
      parsed_headers.push_back(std::move(message_signature));
    }
  }

  return parsed_headers;
}

}  // namespace network
