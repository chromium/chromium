// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"

#include <algorithm>
#include <map>
#include <utility>

#include <bitset>
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/origin.h"

namespace blink {

ParsedFeaturePolicy FeaturePolicyParser::ParseHeader(
    const String& policy,
    scoped_refptr<const SecurityOrigin> origin,
    Vector<String>* messages,
    FeaturePolicyParserDelegate* delegate) {
  return Parse(policy, origin, nullptr, messages, GetDefaultFeatureNameMap(),
               delegate);
}

ParsedFeaturePolicy FeaturePolicyParser::ParseAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    Document* document) {
  return Parse(policy, self_origin, src_origin, messages,
               GetDefaultFeatureNameMap(), document);
}

// normally 1 char = 1 byte
// max length to parse = 2^16 = 64 kB
constexpr wtf_size_t MAX_LENGTH_PARSE = 1 << 16;

ParsedFeaturePolicy FeaturePolicyParser::Parse(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    const FeatureNameMap& feature_names,
    FeaturePolicyParserDelegate* delegate) {
  ParsedFeaturePolicy allowlists;

  if (policy.length() > MAX_LENGTH_PARSE) {
    if (messages) {
      messages->push_back("Feature policy declaration exceeds size limit(" +
                          String::Number(policy.length()) + ">" +
                          String::Number(MAX_LENGTH_PARSE) + ")");
    }
    return allowlists;
  }

  std::bitset<static_cast<size_t>(mojom::FeaturePolicyFeature::kMaxValue) + 1>
      features_specified;
  HashSet<FeaturePolicyAllowlistType> allowlist_types_used;

  // RFC2616, section 4.2 specifies that headers appearing multiple times can be
  // combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  Vector<String> policy_items;
  // policy_items = [ policy *( "," [ policy ] ) ]
  policy.Split(',', policy_items);
  if (policy_items.size() > 1 && delegate) {
    delegate->CountFeaturePolicyUsage(
        mojom::WebFeature::kFeaturePolicyCommaSeparatedDeclarations);
  }
  for (const String& item : policy_items) {
    Vector<String> entry_list;
    // entry_list = [ entry *( ";" [ entry ] ) ]
    item.Split(';', entry_list);
    if (entry_list.size() > 1 && delegate) {
      delegate->CountFeaturePolicyUsage(
          mojom::WebFeature::kFeaturePolicySemicolonSeparatedDeclarations);
    }
    for (const String& entry : entry_list) {
      // Split removes extra whitespaces by default
      //     "name value1 value2" or "name".
      Vector<String> tokens;
      entry.Split(' ', tokens);
      // Empty policy. Skip.
      if (tokens.IsEmpty())
        continue;

      String feature_name = tokens[0];
      if (!feature_names.Contains(feature_name)) {
        if (messages) {
          messages->push_back("Unrecognized feature: '" + tokens[0] + "'.");
        }
        continue;
      }

      if (DisabledByOriginTrial(feature_name, delegate)) {
        if (messages) {
          messages->push_back("Origin trial controlled feature not enabled: '" +
                              tokens[0] + "'.");
        }
        continue;
      }

      mojom::FeaturePolicyFeature feature = feature_names.at(feature_name);
      mojom::PolicyValueType feature_type =
          FeaturePolicy::GetDefaultFeatureList().at(feature).second;
      // If a policy has already been specified for the current feature, drop
      // the new policy.
      if (features_specified[static_cast<size_t>(feature)])
        continue;

      // Count the use of this feature policy.
      if (src_origin) {
        if (!delegate || !delegate->FeaturePolicyFeatureObserved(feature)) {
          UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Allow",
                                    feature);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Header",
                                  feature);
      }

      // True if we should try to detect what kind of allowlist is being used.
      bool count_allowlist_type = true;

      // Flags for the types of items which can be used in allowlists.
      bool allowlist_includes_star = false;
      bool allowlist_includes_self = false;
      bool allowlist_includes_src = false;
      bool allowlist_includes_none = false;
      bool allowlist_includes_origin = false;

      // Detect usage of UnoptimizedImagePolicies origin trial.
      if (feature == mojom::FeaturePolicyFeature::kOversizedImages ||
          feature == mojom::FeaturePolicyFeature::kUnoptimizedLossyImages ||
          feature == mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages ||
          feature ==
              mojom::FeaturePolicyFeature::kUnoptimizedLosslessImagesStrict) {
        if (delegate) {
          delegate->CountFeaturePolicyUsage(
              mojom::WebFeature::kUnoptimizedImagePolicies);
        }
        // Don't analyze allowlists for origin trial features.
        count_allowlist_type = false;
      }

      // Detect usage of UnsizedMediaPolicy origin trial
      if (feature == mojom::FeaturePolicyFeature::kUnsizedMedia) {
        if (delegate) {
          delegate->CountFeaturePolicyUsage(
              mojom::WebFeature::kUnsizedMediaPolicy);
        }
        // Don't analyze allowlists for origin trial features.
        count_allowlist_type = false;
      }

      ParsedFeaturePolicyDeclaration allowlist(feature, feature_type);
      // TODO(loonybear): fallback value should be parsed from the new syntax.
      allowlist.fallback_value = GetFallbackValueForFeature(feature);
      allowlist.opaque_value = GetFallbackValueForFeature(feature);
      features_specified.set(static_cast<size_t>(feature));
      std::map<url::Origin, PolicyValue> values;
      PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
      // If a policy entry has no listed origins (e.g. "feature_name1" in
      // allow="feature_name1; feature_name2 value"), enable the feature for:
      //     a. |self_origin|, if we are parsing a header policy (i.e.,
      //       |src_origin| is null);
      //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
      //       |src_origin| is not null), |src_origin| is not opaque; or
      //     c. the opaque origin of the frame, if |src_origin| is opaque.
      if (tokens.size() == 1) {
        if (!src_origin) {
          values[self_origin->ToUrlOrigin()] = value;
        } else if (!src_origin->IsOpaque()) {
          values[src_origin->ToUrlOrigin()] = value;
        } else {
          allowlist.opaque_value = value;
        }
      }

      for (wtf_size_t i = 1; i < tokens.size(); i++) {
        if (!tokens[i].ContainsOnlyASCIIOrEmpty()) {
          messages->push_back("Non-ASCII characters in origin.");
          continue;
        }

        // Break the token into an origin and a value. Either one may be
        // omitted.
        PolicyValue value = PolicyValue::CreateMaxPolicyValue(feature_type);
        String origin_string = tokens[i];
        String value_string;
        wtf_size_t param_start = origin_string.find('(');
        if (param_start != kNotFound) {
          // There is a value attached to this origin
          if (!origin_string.EndsWith(')')) {
            // The declaration is malformed if the value is not the last part of
            // the string.
            if (messages)
              messages->push_back("Unable to parse policy value.");
            continue;
          }
          value_string = origin_string.Substring(
              param_start + 1, origin_string.length() - param_start - 2);
          origin_string = origin_string.Substring(0, param_start);
          bool ok = false;
          value = ParseValueForType(feature_type, value_string, &ok);
          if (!ok) {
            if (messages)
              messages->push_back("Unable to parse policy value.");
            continue;
          }
        }

        // Determine the target of the declaration. This may be a specific
        // origin, either explicitly written, or one of the special keywords
        // 'self' or 'src'. ('src' can only be used in the iframe allow
        // attribute.)
        url::Origin target_origin;

        // If the iframe will have an opaque origin (for example, if it is
        // sandboxed, or has a data: URL), then 'src' needs to refer to the
        // opaque origin of the frame, which is not known yet. In this case,
        // the |opaque_value| on the declaration is set, rather than adding
        // an origin to the allowlist.
        bool target_is_opaque = false;
        bool target_is_all = false;

        // 'self' origin is used if either the origin is omitted (and there is
        // no 'src' origin available) or the origin is exactly 'self'.
        if ((origin_string.length() == 0 && !src_origin)) {
          target_origin = self_origin->ToUrlOrigin();
        } else if (EqualIgnoringASCIICase(origin_string, "'self'")) {
          target_origin = self_origin->ToUrlOrigin();
          allowlist_includes_self = true;
        }
        // 'src' origin is used if |src_origin| is available and either the
        // origin is omitted or is a match for 'src'. |src_origin| is only set
        // when parsing an iframe allow attribute.
        else if (src_origin &&
                 (origin_string.length() == 0 ||
                  EqualIgnoringASCIICase(origin_string, "'src'"))) {
          if (origin_string.length() > 0)
            allowlist_includes_src = true;
          if (!src_origin->IsOpaque()) {
            target_origin = src_origin->ToUrlOrigin();
          } else {
            target_is_opaque = true;
          }
        } else if (EqualIgnoringASCIICase(origin_string, "'none'")) {
          allowlist_includes_none = true;
          continue;
        } else if (origin_string == "*") {
          target_is_all = true;
          allowlist_includes_star = true;
        }
        // Otherwise, parse the origin string and verify that the result is
        // valid. Invalid strings will produce an opaque origin, which will
        // result in an error message.
        else {
          scoped_refptr<SecurityOrigin> parsed_origin =
              SecurityOrigin::CreateFromString(origin_string);
          if (!parsed_origin->IsOpaque()) {
            target_origin = parsed_origin->ToUrlOrigin();
            allowlist_includes_origin = true;
          } else if (messages) {
            messages->push_back("Unrecognized origin: '" + origin_string +
                                "'.");
            continue;
          }
        }

        // Assign the value to the target origin(s).
        if (target_is_all) {
          allowlist.fallback_value = value;
          allowlist.opaque_value = value;
        } else if (target_is_opaque) {
          allowlist.opaque_value = value;
        } else {
          values[target_origin] = value;
        }
      }

      if (count_allowlist_type) {
        // Record the type of allowlist used.
        if (tokens.size() == 1) {
          allowlist_types_used.insert(FeaturePolicyAllowlistType::kEmpty);
        } else if (tokens.size() == 2) {
          if (allowlist_includes_star)
            allowlist_types_used.insert(FeaturePolicyAllowlistType::kStar);
          else if (allowlist_includes_self)
            allowlist_types_used.insert(FeaturePolicyAllowlistType::kSelf);
          else if (allowlist_includes_src)
            allowlist_types_used.insert(FeaturePolicyAllowlistType::kSrc);
          else if (allowlist_includes_none)
            allowlist_types_used.insert(FeaturePolicyAllowlistType::kNone);
          else
            allowlist_types_used.insert(FeaturePolicyAllowlistType::kOrigins);
        } else {
          if (allowlist_includes_origin) {
            if (allowlist_includes_star || allowlist_includes_none ||
                allowlist_includes_src || allowlist_includes_self)
              allowlist_types_used.insert(FeaturePolicyAllowlistType::kMixed);
            else
              allowlist_types_used.insert(FeaturePolicyAllowlistType::kOrigins);
          } else {
            allowlist_types_used.insert(
                FeaturePolicyAllowlistType::kKeywordsOnly);
          }
        }
      }

      // Size reduction: remove all items in the allowlist whose value is the
      // same as the fallback.
      for (auto it = values.begin(); it != values.end();) {
        if (it->second == allowlist.fallback_value)
          it = values.erase(it);
        else
          it++;
      }

      allowlist.values = std::move(values);
      allowlists.push_back(allowlist);
    }
  }

  // The use of various allowlist types should only be recorded once per page.
  // For simplicity, this recording assumes that the ParseHeader method is
  // called once when creating a new document, and similarly the ParseAttribute
  // method is called once for a frame. It is possible for multiple calls, but
  // the additional complexity to guarantee only one record isn't warranted as
  // yet.
  for (const FeaturePolicyAllowlistType allowlist_type : allowlist_types_used) {
    if (src_origin) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.AttributeAllowlistType",
          allowlist_type);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.HeaderAllowlistType", allowlist_type);
    }
  }
  return allowlists;
}

// TODO(loonybear): once the new syntax is implemented, use this method to
// parse the policy value for each parameterized feature, and for non
// parameterized feature (i.e. boolean-type policy value).
PolicyValue FeaturePolicyParser::GetFallbackValueForFeature(
    mojom::FeaturePolicyFeature feature) {
  if (feature == mojom::FeaturePolicyFeature::kOversizedImages) {
    return PolicyValue(2.0);
  }
  if (feature == mojom::FeaturePolicyFeature::kUnoptimizedLossyImages) {
    // Lossy images default to at most 0.5 bytes per pixel.
    return PolicyValue(0.5);
  }
  if (feature == mojom::FeaturePolicyFeature::kUnoptimizedLosslessImages ||
      feature ==
          mojom::FeaturePolicyFeature::kUnoptimizedLosslessImagesStrict) {
    // Lossless images default to at most 1 byte per pixel.
    return PolicyValue(1.0);
  }

  return PolicyValue(false);
}

PolicyValue FeaturePolicyParser::ParseValueForType(
    mojom::PolicyValueType feature_type,
    const String& value_string,
    bool* ok) {
  *ok = false;
  PolicyValue value;
  switch (feature_type) {
    case mojom::PolicyValueType::kBool:
      // recognize true, false
      if (value_string.LowerASCII() == "true") {
        value = PolicyValue(true);
        *ok = true;
      } else if (value_string.LowerASCII() == "false") {
        value = PolicyValue(false);
        *ok = true;
      }
      break;
    case mojom::PolicyValueType::kDecDouble: {
      if (value_string.LowerASCII() == "inf") {
        value = PolicyValue::CreateMaxPolicyValue(feature_type);
        *ok = true;
      } else {
        double parsed_value = value_string.ToDouble(ok);
        if (*ok && parsed_value >= 0.0f) {
          value = PolicyValue(parsed_value);
        } else {
          *ok = false;
        }
      }
      break;
    }
    default:
      NOTREACHED();
  }
  if (!*ok)
    return PolicyValue();
  return value;
}

void FeaturePolicyParser::ParseValueForFuzzer(
    blink::mojom::PolicyValueType feature_type,
    const WTF::String& value_string) {
  bool ok;
  ParseValueForType(feature_type, value_string, &ok);
}

bool IsFeatureDeclared(mojom::FeaturePolicyFeature feature,
                       const ParsedFeaturePolicy& policy) {
  return std::any_of(policy.begin(), policy.end(),
                     [feature](const auto& declaration) {
                       return declaration.feature == feature;
                     });
}

bool RemoveFeatureIfPresent(mojom::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  auto new_end = std::remove_if(policy.begin(), policy.end(),
                                [feature](const auto& declaration) {
                                  return declaration.feature == feature;
                                });
  if (new_end == policy.end())
    return false;
  policy.erase(new_end, policy.end());
  return true;
}

bool DisallowFeatureIfNotPresent(mojom::FeaturePolicyFeature feature,
                                 ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  blink::mojom::PolicyValueType feature_type =
      blink::FeaturePolicy::GetDefaultFeatureList().at(feature).second;
  ParsedFeaturePolicyDeclaration allowlist(feature, feature_type);
  policy.push_back(allowlist);
  return true;
}

bool AllowFeatureEverywhereIfNotPresent(mojom::FeaturePolicyFeature feature,
                                        ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  blink::mojom::PolicyValueType feature_type =
      blink::FeaturePolicy::GetDefaultFeatureList().at(feature).second;
  ParsedFeaturePolicyDeclaration allowlist(feature, feature_type);
  allowlist.fallback_value.SetToMax();
  allowlist.opaque_value.SetToMax();
  policy.push_back(allowlist);
  return true;
}

void DisallowFeature(mojom::FeaturePolicyFeature feature,
                     ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  DisallowFeatureIfNotPresent(feature, policy);
}

void AllowFeatureEverywhere(mojom::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  AllowFeatureEverywhereIfNotPresent(feature, policy);
}

const Vector<String> GetAvailableFeatures(ExecutionContext* execution_context) {
  Vector<String> available_features;
  for (const auto& feature : GetDefaultFeatureNameMap()) {
    if (!DisabledByOriginTrial(feature.key, execution_context))
      available_features.push_back(feature.key);
  }
  return available_features;
}

const String& GetNameForFeature(mojom::FeaturePolicyFeature feature) {
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (entry.value == feature)
      return entry.key;
  }
  return g_empty_string;
}

}  // namespace blink
