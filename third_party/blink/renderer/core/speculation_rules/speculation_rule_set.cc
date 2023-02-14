// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
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

// If `out_error` is provided and hasn't already had a message set, sets it to
// `message`.
void SetParseErrorMessage(String* out_error, String message) {
  if (out_error && out_error->IsNull()) {
    *out_error = message;
  }
}

SpeculationRule* ParseSpeculationRule(JSONObject* input,
                                      const KURL& base_url,
                                      ExecutionContext* context,
                                      String* out_error) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-a-speculation-rule

  // If input has any key other than "source", "urls", "requires", "target_hint"
  // and "relative_to", then return null.
  const char* const kKnownKeys[] = {"source",      "urls",  "requires",
                                    "target_hint", "where", "relative_to"};
  const auto kConditionalKnownKeys = [context]() {
    Vector<const char*, 4> conditional_known_keys;
    if (RuntimeEnabledFeatures::SpeculationRulesReferrerPolicyKeyEnabled(
            context)) {
      conditional_known_keys.push_back("referrer_policy");
    }
    if (RuntimeEnabledFeatures::SpeculationRulesEagernessEnabled(context)) {
      conditional_known_keys.push_back("eagerness");
    }
    return conditional_known_keys;
  }();

  for (wtf_size_t i = 0; i < input->size(); ++i) {
    const String& input_key = input->at(i).first;
    if (!base::Contains(kKnownKeys, input_key) &&
        !base::Contains(kConditionalKnownKeys, input_key)) {
      SetParseErrorMessage(
          out_error, "A rule contains an unknown key: \"" + input_key + "\".");
      return nullptr;
    }
  }

  bool document_rules_enabled =
      RuntimeEnabledFeatures::SpeculationRulesDocumentRulesEnabled(context);
  const bool relative_to_enabled =
      RuntimeEnabledFeatures::SpeculationRulesRelativeToDocumentEnabled(
          context);

  // If input["source"] does not exist or is neither the string "list" nor the
  // string "document", then return null.
  String source;
  if (!input->GetString("source", &source)) {
    SetParseErrorMessage(out_error, "A rule must have a source.");
    return nullptr;
  }
  if (!(source == "list" || (document_rules_enabled && source == "document"))) {
    SetParseErrorMessage(out_error,
                         "A rule has an unknown source: \"" + source + "\".");
    return nullptr;
  }

  Vector<KURL> urls;
  if (source == "list") {
    // If input["where"] exists, then return null.
    if (input->Get("where")) {
      SetParseErrorMessage(out_error,
                           "A list rule may not have document rule matchers.");
      return nullptr;
    }

    // For now, use the given base URL to construct the list rules.
    KURL base_url_to_parse = base_url;
    //  If input["relative_to"] exists:
    if (JSONValue* relative_to = input->Get("relative_to")) {
      const char* const kKnownRelativeToValues[] = {"ruleset", "document"};
      String value;
      // If relativeTo is neither the string "ruleset" nor the string
      // "document", then return null.
      if (!relative_to_enabled || !relative_to->AsString(&value) ||
          !base::Contains(kKnownRelativeToValues, value)) {
        SetParseErrorMessage(out_error,
                             "A rule has an unknown \"relative_to\" value.");
        return nullptr;
      }
      // If relativeTo is "document", then set baseURL to the document's
      // document base URL.
      if (value == "document") {
        base_url_to_parse = context->BaseURL();
      }
    }

    // Let urls be an empty list.
    // If input["urls"] does not exist, is not a list, or has any element which
    // is not a string, then return null.
    JSONArray* input_urls = input->GetArray("urls");
    if (!input_urls) {
      SetParseErrorMessage(out_error,
                           "A list rule must have a \"urls\" array.");
      return nullptr;
    }

    // For each urlString of input["urls"]...
    urls.ReserveInitialCapacity(input_urls->size());
    for (wtf_size_t i = 0; i < input_urls->size(); ++i) {
      String url_string;
      if (!input_urls->at(i)->AsString(&url_string)) {
        SetParseErrorMessage(out_error, "URLs must be given as strings.");
        return nullptr;
      }

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
    if (input->Get("urls")) {
      SetParseErrorMessage(out_error,
                           "A document rule cannot have a \"urls\" key.");
      return nullptr;
    }

    // "relative_to" outside the "href_matches" clause is not allowed for
    // document rules.
    if (input->Get("relative_to")) {
      SetParseErrorMessage(out_error,
                           "A document rule cannot have \"relative_to\" "
                           "outside the \"where\" clause.");
      return nullptr;
    }

    // If input["where"] does not exist, then set predicate to a document rule
    // conjunction whose clauses is an empty list.
    if (!input->Get("where")) {
      document_rule_predicate = DocumentRulePredicate::MakeDefaultPredicate();
    }
    // Otherwise, set predicate to the result of parsing a document rule
    // predicate given input["where"] and baseURL.
    else {
      document_rule_predicate = DocumentRulePredicate::Parse(
          input->GetJSONObject("where"), base_url, context,
          IGNORE_EXCEPTION_FOR_TESTING, out_error);
    }
    if (!document_rule_predicate)
      return nullptr;
  }

  // Let requirements be an empty ordered set.
  // If input["requires"] exists, but is not a list, then return null.
  JSONValue* requirements = input->Get("requires");
  if (requirements && requirements->GetType() != JSONValue::kTypeArray) {
    SetParseErrorMessage(out_error, "\"requires\" must be an array.");
    return nullptr;
  }

  // For each requirement of input["requires"]...
  SpeculationRule::RequiresAnonymousClientIPWhenCrossOrigin
      requires_anonymous_client_ip(false);
  if (JSONArray* requirements_array = JSONArray::Cast(requirements)) {
    for (wtf_size_t i = 0; i < requirements_array->size(); ++i) {
      String requirement;
      if (!requirements_array->at(i)->AsString(&requirement)) {
        SetParseErrorMessage(out_error, "Requirements must be strings.");
        return nullptr;
      }

      if (requirement == "anonymous-client-ip-when-cross-origin") {
        requires_anonymous_client_ip =
            SpeculationRule::RequiresAnonymousClientIPWhenCrossOrigin(true);
      } else {
        SetParseErrorMessage(
            out_error,
            "A rule has an unknown requirement: \"" + requirement + "\".");
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
    if (!target_hint_value->AsString(&target_hint_str)) {
      SetParseErrorMessage(out_error, "\"target_hint\" must be a string.");
      return nullptr;
    }
    if (!IsValidBrowsingContextNameOrKeyword(target_hint_str)) {
      SetParseErrorMessage(out_error,
                           "A rule has an invalid \"target_hint\": \"" +
                               target_hint_str + "\".");
      return nullptr;
    }
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
    if (!referrer_policy_value->AsString(&referrer_policy_str)) {
      SetParseErrorMessage(out_error, "A referrer policy must be a string.");
      return nullptr;
    }

    if (!referrer_policy_str.empty()) {
      network::mojom::ReferrerPolicy referrer_policy_out =
          network::mojom::ReferrerPolicy::kDefault;
      if (!SecurityPolicy::ReferrerPolicyFromString(
              referrer_policy_str, kDoNotSupportReferrerPolicyLegacyKeywords,
              &referrer_policy_out)) {
        SetParseErrorMessage(out_error,
                             "A rule has an invalid referrer policy: \"" +
                                 referrer_policy_str + "\".");
        return nullptr;
      }
      DCHECK_NE(referrer_policy_out, network::mojom::ReferrerPolicy::kDefault);
      referrer_policy = referrer_policy_out;
    }
  }

  absl::optional<mojom::blink::SpeculationEagerness> eagerness;
  if (JSONValue* eagerness_value = input->Get("eagerness")) {
    // Feature gated due to known keys check above.
    DCHECK(RuntimeEnabledFeatures::SpeculationRulesEagernessEnabled(context));

    String eagerness_str;
    if (!eagerness_value->AsString(&eagerness_str)) {
      SetParseErrorMessage(out_error, "Eagerness value must be a string.");
      return nullptr;
    }

    if (eagerness_str == "eager") {
      eagerness = mojom::blink::SpeculationEagerness::kEager;
    } else if (eagerness_str == "moderate") {
      eagerness = mojom::blink::SpeculationEagerness::kModerate;
    } else if (eagerness_str == "conservative") {
      eagerness = mojom::blink::SpeculationEagerness::kConservative;
    } else {
      SetParseErrorMessage(
          out_error, "Eagerness value: \"" + eagerness_str + "\" is invalid.");
      return nullptr;
    }
  }

  return MakeGarbageCollected<SpeculationRule>(
      std::move(urls), document_rule_predicate, requires_anonymous_client_ip,
      target_hint, referrer_policy, eagerness);
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

namespace {

// If enabled, allows non-standard JSON comments in speculation rules.
// TODO(crbug.com/1264024): Remove this feature if no issues arose with
// deprecating it.
BASE_FEATURE(kSpeculationRulesJSONComments,
             "SpeculationRulesJSONComments",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

SpeculationRuleSet::SpeculationRuleSet(base::PassKey<SpeculationRuleSet>,
                                       Source* source)
    : inspector_id_(IdentifiersFactory::CreateIdentifier()), source_(source) {}

// static
SpeculationRuleSet* SpeculationRuleSet::Parse(Source* source,
                                              ExecutionContext* context,
                                              String* out_error) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-speculation-rules

  const String& source_text = source->GetSourceText();
  const KURL& base_url = source->GetBaseURL();

  // Let parsed be the result of parsing a JSON string to an Infra value given
  // input.
  // TODO(crbug.com/1264024): Deprecate JSON comments here, if possible.
  JSONParseError parse_error;
  auto parsed = JSONObject::From(
      base::FeatureList::IsEnabled(kSpeculationRulesJSONComments)
          ? ParseJSONWithCommentsDeprecated(source_text, &parse_error)
          : ParseJSON(source_text, &parse_error));

  // If parsed is not a map, then return null.
  if (!parsed) {
    SetParseErrorMessage(out_error,
                         parse_error.type != JSONParseErrorType::kNoError
                             ? parse_error.message
                             : "Parsed JSON must be an object.");
    return nullptr;
  }

  // Let result be an empty speculation rule set.
  SpeculationRuleSet* result = MakeGarbageCollected<SpeculationRuleSet>(
      base::PassKey<SpeculationRuleSet>(), source);

  const auto parse_for_action =
      [&](const char* key, HeapVector<Member<SpeculationRule>>& destination,
          bool allow_target_hint) {
        JSONArray* array = parsed->GetArray(key);
        if (!array) {
          return;
        }

        for (wtf_size_t i = 0; i < array->size(); ++i) {
          // If prefetch/prerenderRule is not a map, then continue.
          JSONObject* input_rule = JSONObject::Cast(array->at(i));
          if (!input_rule) {
            SetParseErrorMessage(out_error, "A rule must be an object.");
            continue;
          }

          // Let rule be the result of parsing a speculation rule given
          // prefetch/prerenderRule and baseURL.
          SpeculationRule* rule =
              ParseSpeculationRule(input_rule, base_url, context, out_error);

          // If rule is null, then continue.
          if (!rule)
            continue;

          // If rule's target browsing context name hint is not null, then
          // continue.
          if (!allow_target_hint &&
              rule->target_browsing_context_name_hint().has_value()) {
            SetParseErrorMessage(
                out_error, "\"target_hint\" may not be set for " + String(key) +
                               " rules.");
            continue;
          }

          if (rule->predicate()) {
            result->has_document_rule_ = true;
            result->selectors_.AppendVector(rule->predicate()->GetStyleRules());
          }

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
  visitor->Trace(selectors_);
}

}  // namespace blink
