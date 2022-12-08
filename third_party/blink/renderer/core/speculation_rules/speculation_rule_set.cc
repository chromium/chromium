// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/containers/contains.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

// https://html.spec.whatwg.org/C/#valid-browsing-context-name
bool IsValidContextName(const String& name_or_keyword) {
  // "A valid browsing context name is any string with at least one character
  // that does not start with a U+005F LOW LINE character. (Names starting with
  // an underscore are reserved for special keywords.)"
  if (name_or_keyword.empty())
    return false;
  if (name_or_keyword.StartsWith("_"))
    return false;
  return true;
}

// https://html.spec.whatwg.org/C/#valid-browsing-context-name-or-keyword
bool IsValidBrowsingContextNameOrKeyword(const String& name_or_keyword) {
  // "A valid browsing context name or keyword is any string that is either a
  // valid browsing context name or that is an ASCII case-insensitive match for
  // one of: _blank, _self, _parent, or _top."
  String canonicalized_name_or_keyword = name_or_keyword.LowerASCII();
  if (IsValidContextName(name_or_keyword) ||
      EqualIgnoringASCIICase(name_or_keyword, "_blank") ||
      EqualIgnoringASCIICase(name_or_keyword, "_self") ||
      EqualIgnoringASCIICase(name_or_keyword, "_parent") ||
      EqualIgnoringASCIICase(name_or_keyword, "_top")) {
    return true;
  }
  return false;
}

SpeculationRule* ParseSpeculationRule(JSONObject* input,
                                      const KURL& base_url,
                                      ExecutionContext* context) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-a-speculation-rule

  // If input has any key other than "source", "urls", "requires", "target_hint"
  // and "relative_to", then return null.
  const char* const kKnownKeys[] = {"source",      "urls",  "requires",
                                    "target_hint", "where", "relative_to"};
  for (wtf_size_t i = 0; i < input->size(); ++i) {
    const String& input_key = input->at(i).first;
    const bool conditionally_known_key =
        RuntimeEnabledFeatures::SpeculationRulesReferrerPolicyKeyEnabled(
            context) &&
        input_key == "referrer_policy";
    if (!base::Contains(kKnownKeys, input_key) && !conditionally_known_key)
      return nullptr;
  }

  bool document_rules_enabled =
      RuntimeEnabledFeatures::SpeculationRulesDocumentRulesEnabled(context);
  const bool relative_to_enabled =
      RuntimeEnabledFeatures::SpeculationRulesRelativeToDocumentEnabled(
          context);

  // If input["source"] does not exist or is neither the string "list" nor the
  // string "document", then return null.
  String source;
  if (!input->GetString("source", &source) ||
      !(source == "list" || (document_rules_enabled && source == "document")))
    return nullptr;

  Vector<KURL> urls;
  if (source == "list") {
    // If input["where"] exists, then return null.
    if (input->Get("where"))
      return nullptr;

    // For now, use the given base URL to construct the list rules.
    KURL base_url_to_parse = base_url;
    // If "relative_to" is present:
    if (JSONValue* relative_to = input->Get("relative_to")) {
      // If the value of "relative_to" is not a string, or the string value is
      // not "document", the ruleset is invalid.
      if (String value; !relative_to_enabled ||
                        !relative_to->AsString(&value) || value != "document") {
        return nullptr;
      }
      // If "relative_to": "document" is present, use the document's base URL to
      // construct the list rules.
      base_url_to_parse = context->BaseURL();
    }

    // Let urls be an empty list.
    // If input["urls"] does not exist, is not a list, or has any element which
    // is not a string, then return null.
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
      KURL parsed_url(base_url_to_parse, url_string);
      if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHTTPFamily())
        continue;

      urls.push_back(std::move(parsed_url));
    }
  }

  DocumentRulePredicate* document_rule_predicate = nullptr;
  if (source == "document") {
    DCHECK(document_rules_enabled);
    // If input["urls"] exists, then return null.
    if (input->Get("urls"))
      return nullptr;

    // If input["where"] does not exist, then set predicate to a document rule
    // conjunction whose clauses is an empty list.
    if (!input->Get("where")) {
      document_rule_predicate = DocumentRulePredicate::MakeDefaultPredicate();
    }
    // Otherwise, set predicate to the result of parsing a document rule
    // predicate given input["where"] and baseURL.
    else {
      // "relative_to" outside the "href_matches" clause is not allowed for
      // document rules.
      if (input->Get("relative_to"))
        return nullptr;
      document_rule_predicate =
          DocumentRulePredicate::Parse(input->GetJSONObject("where"), base_url,
                                       context, IGNORE_EXCEPTION_FOR_TESTING);
    }
    if (!document_rule_predicate)
      return nullptr;
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

  // Let targetHint be null.
  absl::optional<mojom::blink::SpeculationTargetHint> target_hint;

  // If input["target_hint"] exists:
  JSONValue* target_hint_value = input->Get("target_hint");
  if (target_hint_value) {
    // If input["target_hint"] is not a valid browsing context name or keyword,
    // then return null.
    // Set targetHint to input["target_hint"].
    String target_hint_str;
    if (!target_hint_value->AsString(&target_hint_str))
      return nullptr;
    if (!IsValidBrowsingContextNameOrKeyword(target_hint_str))
      return nullptr;
    // Currently only "_blank" and "_self" are supported.
    // TODO(https://crbug.com/1354049): Support more browsing context names and
    // keywords.
    if (EqualIgnoringASCIICase(target_hint_str, "_blank")) {
      target_hint = mojom::blink::SpeculationTargetHint::kBlank;
    } else if (EqualIgnoringASCIICase(target_hint_str, "_self")) {
      target_hint = mojom::blink::SpeculationTargetHint::kSelf;
    } else {
      target_hint = mojom::blink::SpeculationTargetHint::kNoHint;
    }
  }

  absl::optional<network::mojom::ReferrerPolicy> referrer_policy;
  JSONValue* referrer_policy_value = input->Get("referrer_policy");
  if (referrer_policy_value) {
    // Feature gated due to known keys check above.
    DCHECK(RuntimeEnabledFeatures::SpeculationRulesReferrerPolicyKeyEnabled(
        context));

    String referrer_policy_str;
    if (!referrer_policy_value->AsString(&referrer_policy_str))
      return nullptr;

    if (!referrer_policy_str.empty()) {
      network::mojom::ReferrerPolicy referrer_policy_out =
          network::mojom::ReferrerPolicy::kDefault;
      if (!SecurityPolicy::ReferrerPolicyFromString(
              referrer_policy_str, kDoNotSupportReferrerPolicyLegacyKeywords,
              &referrer_policy_out)) {
        return nullptr;
      }
      DCHECK_NE(referrer_policy_out, network::mojom::ReferrerPolicy::kDefault);
      referrer_policy = referrer_policy_out;
    }
  }

  return MakeGarbageCollected<SpeculationRule>(
      std::move(urls), document_rule_predicate, requires_anonymous_client_ip,
      target_hint, referrer_policy);
}

}  // namespace

// ---- SpeculationRuleSet::Source implementation ----

SpeculationRuleSet::Source::Source(const String& source_text,
                                   Document& document)
    : source_text_(source_text),
      base_url_(absl::nullopt),
      document_(document) {}

SpeculationRuleSet::Source::Source(const String& source_text,
                                   const KURL& base_url)
    : source_text_(source_text), base_url_(base_url), document_(nullptr) {}

const String& SpeculationRuleSet::Source::GetSourceText() const {
  return source_text_;
}

KURL SpeculationRuleSet::Source::GetBaseURL() const {
  if (base_url_) {
    DCHECK(!document_);
    return base_url_.value();
  }
  DCHECK(document_);
  return document_->BaseURL();
}

void SpeculationRuleSet::Source::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

// ---- SpeculationRuleSet implementation ----

// static
SpeculationRuleSet* SpeculationRuleSet::Parse(Source* source,
                                              ExecutionContext* context,
                                              String* out_error) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-speculation-rules

  const String& source_text = source->GetSourceText();
  const KURL& base_url = source->GetBaseURL();

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
  result->source_ = source;

  const auto parse_for_action =
      [&](const char* key, HeapVector<Member<SpeculationRule>>& destination,
          bool allow_target_hint) {
        JSONArray* array = parsed->GetArray(key);
        if (!array)
          return;

        for (wtf_size_t i = 0; i < array->size(); ++i) {
          // If prefetch/prerenderRule is not a map, then continue.
          JSONObject* input_rule = JSONObject::Cast(array->at(i));
          if (!input_rule)
            continue;

          // Let rule be the result of parsing a speculation rule given
          // prefetch/prerenderRule and baseURL.
          SpeculationRule* rule =
              ParseSpeculationRule(input_rule, base_url, context);

          // If rule is null, then continue.
          if (!rule)
            continue;

          // If rule's target browsing context name hint is not null, then
          // continue.
          if (!allow_target_hint &&
              rule->target_browsing_context_name_hint().has_value()) {
            continue;
          }

          if (rule->predicate())
            result->has_document_rule_ = true;

          // Append rule to result's prefetch/prerender rules.
          destination.push_back(rule);
        }
      };

  // If parsed["prefetch"] exists and is a list, then for each...
  parse_for_action("prefetch", result->prefetch_rules_, false);

  // If parsed["prefetch_with_subresources"] exists and is a list, then for
  // each...
  parse_for_action("prefetch_with_subresources",
                   result->prefetch_with_subresources_rules_, false);

  // If parsed["prerender"] exists and is a list, then for each...
  parse_for_action("prerender", result->prerender_rules_, true);

  return result;
}

void SpeculationRuleSet::Trace(Visitor* visitor) const {
  visitor->Trace(prefetch_rules_);
  visitor->Trace(prefetch_with_subresources_rules_);
  visitor->Trace(prerender_rules_);
  visitor->Trace(source_);
}

}  // namespace blink
