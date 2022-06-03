// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

SpeculationRule* ParseSpeculationRule(JSONObject* input, const KURL& base_url) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-a-speculation-rule

  // If input has any key other than "source", "urls" and "requires", then
  // return null.
  const char* const kKnownKeys[] = {"source", "urls", "requires"};
  for (wtf_size_t i = 0; i < input->size(); ++i) {
    if (!base::Contains(kKnownKeys, input->at(i).first))
      return nullptr;
  }

  // If input["source"] does not exist or is not the string "list", then return
  // null.
  String source;
  if (!input->GetString("source", &source) || source != "list")
    return nullptr;

  // Let urls be an empty list.
  // If input["urls"] does not exist, is not a list, or has any element which is
  // not a string, then return null.
  Vector<KURL> urls;
  JSONArray* input_urls = input->GetArray("urls");
  if (!input_urls)
    return nullptr;

  // For each urlString of input["urls"]...
  urls.ReserveInitialCapacity(input_urls->size());
  for (wtf_size_t i = 0; i < input_urls->size(); ++i) {
    String url_string;
    if (!input_urls->at(i)->AsString(&url_string))
      return nullptr;

    // Let parsedURL be the result of parsing urlString with baseURL.
    // If parsedURL is failure, then continue.
    KURL parsed_url(base_url, url_string);
    if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHTTPFamily())
      continue;

    urls.push_back(std::move(parsed_url));
  }

  // Let requirements be an empty ordered set.
  // If input["requires"] exists, but is not a list, then return null.
  JSONValue* requirements = input->Get("requires");
  if (requirements && requirements->GetType() != JSONValue::kTypeArray)
    return nullptr;

  // For each requirement of input["requires"]...
  SpeculationRule::RequiresAnonymousClientIPWhenCrossOrigin
      requires_anonymous_client_ip(false);
  if (JSONArray* requirements_array = JSONArray::Cast(requirements)) {
    for (wtf_size_t i = 0; i < requirements_array->size(); ++i) {
      String requirement;
      if (!requirements_array->at(i)->AsString(&requirement))
        return nullptr;

      if (requirement == "anonymous-client-ip-when-cross-origin") {
        requires_anonymous_client_ip =
            SpeculationRule::RequiresAnonymousClientIPWhenCrossOrigin(true);
      } else {
        return nullptr;
      }
    }
  }

  return MakeGarbageCollected<SpeculationRule>(std::move(urls),
                                               requires_anonymous_client_ip);
}

}  // namespace

// static
SpeculationRuleSet* SpeculationRuleSet::ParseInline(const String& source_text,
                                                    const KURL& base_url,
                                                    String* out_error) {
  // https://wicg.github.io/nav-speculation/prerendering.html#parse-speculation-rules

  // Let parsed be the result of parsing a JSON string to an Infra value given
  // input.
  JSONParseError parse_error;
  auto parsed = JSONObject::From(ParseJSON(source_text, &parse_error));

  // If parsed is not a map, then return null.
  if (!parsed) {
    if (out_error) {
      *out_error = parse_error.type != JSONParseErrorType::kNoError
                       ? parse_error.message
                       : "Parsed JSON must be an object.";
    }
    return nullptr;
  }

  // Let result be an empty speculation rule set.
  SpeculationRuleSet* result = MakeGarbageCollected<SpeculationRuleSet>();

  const auto parse_for_action =
      [&](const char* key, HeapVector<Member<SpeculationRule>>& destination) {
        JSONArray* array = parsed->GetArray(key);
        if (!array)
          return;

        for (wtf_size_t i = 0; i < array->size(); ++i) {
          JSONObject* input_rule = JSONObject::Cast(array->at(i));
          if (!input_rule)
            continue;

          if (SpeculationRule* r = ParseSpeculationRule(input_rule, base_url))
            destination.push_back(r);
        }
      };

  // If parsed["prefetch"] exists and is a list, then for each...
  parse_for_action("prefetch", result->prefetch_rules_);

  // If parsed["prefetch_with_subresources"] exists and is a list, then for
  // each...
  parse_for_action("prefetch_with_subresources",
                   result->prefetch_with_subresources_rules_);

  // If parsed["prerender"] exists and is a list, then for each...
  parse_for_action("prerender", result->prerender_rules_);
  return result;
}

void SpeculationRuleSet::Trace(Visitor* visitor) const {
  visitor->Trace(prefetch_rules_);
  visitor->Trace(prefetch_with_subresources_rules_);
  visitor->Trace(prerender_rules_);
}

}  // namespace blink
