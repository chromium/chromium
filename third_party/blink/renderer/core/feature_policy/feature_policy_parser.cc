// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"

#include <algorithm>
#include <utility>

#include <bitset>
#include "base/metrics/histogram_macros.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
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
namespace {

class ParsedFeaturePolicies final
    : public GarbageCollected<ParsedFeaturePolicies>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  static ParsedFeaturePolicies& From(ExecutionContext& context) {
    ParsedFeaturePolicies* policies =
        Supplement<ExecutionContext>::From<ParsedFeaturePolicies>(context);
    if (!policies) {
      policies = MakeGarbageCollected<ParsedFeaturePolicies>(context);
      Supplement<ExecutionContext>::ProvideTo(context, policies);
    }
    return *policies;
  }

  explicit ParsedFeaturePolicies(ExecutionContext& context)
      : Supplement<ExecutionContext>(context),
        policies_(
            static_cast<size_t>(mojom::blink::FeaturePolicyFeature::kMaxValue) +
            1) {}

  bool Observed(mojom::blink::FeaturePolicyFeature feature) {
    size_t feature_index = static_cast<size_t>(feature);
    if (policies_[feature_index])
      return true;
    policies_[feature_index] = true;
    return false;
  }

 private:
  // Tracks which feature policies have already been parsed, so as not to count
  // them multiple times.
  Vector<bool> policies_;
};

const char ParsedFeaturePolicies::kSupplementName[] = "ParsedFeaturePolicies";

class FeatureObserver {
 public:
  // Returns whether the feature has been observed before or not.
  bool FeatureObserved(mojom::blink::FeaturePolicyFeature feature);

 private:
  std::bitset<
      static_cast<size_t>(mojom::blink::FeaturePolicyFeature::kMaxValue) + 1>
      features_specified_;
};

class ParsingContext {
  STACK_ALLOCATED();

 public:
  ParsingContext(PolicyParserMessageBuffer& logger,
                 scoped_refptr<const SecurityOrigin> self_origin,
                 scoped_refptr<const SecurityOrigin> src_origin,
                 const FeatureNameMap& feature_names,
                 ExecutionContext* execution_context)
      : logger_(logger),
        self_origin_(self_origin),
        src_origin_(src_origin),
        feature_names_(feature_names),
        execution_context_(execution_context) {}

  ~ParsingContext() = default;

  ParsedFeaturePolicy ParseFeaturePolicy(const String& policy);
  ParsedFeaturePolicy ParsePermissionsPolicy(const String& policy);

 private:
  // Following is the intermediate represetnation(IR) of feature policy.
  // Parsing of syntax structures is done in this IR, but semantic checks, e.g.
  // whether feature_name is valid, are not yet performed.
  struct FeaturePolicyDeclarationNode {
    String feature_name;
    Vector<String> allowlist;
  };
  using FeaturePolicyNode = Vector<FeaturePolicyDeclarationNode>;

  ParsedFeaturePolicy ParseIR(const FeaturePolicyNode& root);
  FeaturePolicyNode ParseFeaturePolicyToIR(const String& policy);
  FeaturePolicyNode ParsePermissionsPolicyToIR(const String& policy);

  // normally 1 char = 1 byte
  // max length to parse = 2^16 = 64 kB
  static constexpr wtf_size_t MAX_LENGTH_PARSE = 1 << 16;

  base::Optional<ParsedFeaturePolicyDeclaration> ParseFeature(
      const FeaturePolicyDeclarationNode&);

  struct ParsedAllowlist {
    std::vector<url::Origin> allowed_origins;
    bool matches_all_origins{false};
    bool matches_opaque_src{false};

    ParsedAllowlist() : allowed_origins({}) {}
  };

  base::Optional<mojom::blink::FeaturePolicyFeature> ParseFeatureName(
      const String& feature_name);

  // Parse allowlist for feature.
  ParsedAllowlist ParseAllowlist(const Vector<String>& origin_strings);

  void ReportFeatureUsage(mojom::blink::FeaturePolicyFeature feature);

  // This function should be called after Allowlist Histograms related flags
  // have been captured.
  void RecordAllowlistTypeUsage(size_t origin_count);
  // The use of various allowlist types should only be recorded once per page.
  // For simplicity, this recording assumes that the ParseHeader method is
  // called once when creating a new document, and similarly the ParseAttribute
  // method is called once for a frame. It is possible for multiple calls, but
  // the additional complexity to guarantee only one record isn't warranted as
  // yet.
  void ReportAllowlistTypeUsage();

  PolicyParserMessageBuffer& logger_;
  scoped_refptr<const SecurityOrigin> self_origin_;
  scoped_refptr<const SecurityOrigin> src_origin_;
  const FeatureNameMap& feature_names_;
  ExecutionContext* execution_context_;

  // Flags for the types of items which can be used in allowlists.
  bool allowlist_includes_star_ = false;
  bool allowlist_includes_self_ = false;
  bool allowlist_includes_src_ = false;
  bool allowlist_includes_none_ = false;
  bool allowlist_includes_origin_ = false;

  HashSet<FeaturePolicyAllowlistType> allowlist_types_used_;

  FeatureObserver feature_observer_;
};

bool FeatureObserver::FeatureObserved(
    mojom::blink::FeaturePolicyFeature feature) {
  if (features_specified_[static_cast<size_t>(feature)]) {
    return true;
  } else {
    features_specified_.set(static_cast<size_t>(feature));
    return false;
  }
}

void ParsingContext::ReportFeatureUsage(
    mojom::blink::FeaturePolicyFeature feature) {
  if (src_origin_) {
    if (!execution_context_ ||
        !ParsedFeaturePolicies::From(*execution_context_).Observed(feature)) {
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Allow",
                                feature);
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Header", feature);
  }
}

void ParsingContext::RecordAllowlistTypeUsage(size_t origin_count) {
  // Record the type of allowlist used.
  if (origin_count == 0) {
    allowlist_types_used_.insert(FeaturePolicyAllowlistType::kEmpty);
  } else if (origin_count == 1) {
    if (allowlist_includes_star_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kStar);
    else if (allowlist_includes_self_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kSelf);
    else if (allowlist_includes_src_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kSrc);
    else if (allowlist_includes_none_)
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kNone);
    else
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kOrigins);
  } else {
    if (allowlist_includes_origin_) {
      if (allowlist_includes_star_ || allowlist_includes_none_ ||
          allowlist_includes_src_ || allowlist_includes_self_)
        allowlist_types_used_.insert(FeaturePolicyAllowlistType::kMixed);
      else
        allowlist_types_used_.insert(FeaturePolicyAllowlistType::kOrigins);
    } else {
      allowlist_types_used_.insert(FeaturePolicyAllowlistType::kKeywordsOnly);
    }
  }
  // Reset all flags.
  allowlist_includes_star_ = false;
  allowlist_includes_self_ = false;
  allowlist_includes_src_ = false;
  allowlist_includes_none_ = false;
  allowlist_includes_origin_ = false;
}

void ParsingContext::ReportAllowlistTypeUsage() {
  for (const FeaturePolicyAllowlistType allowlist_type :
       allowlist_types_used_) {
    if (src_origin_) {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.AttributeAllowlistType",
          allowlist_type);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          "Blink.UseCounter.FeaturePolicy.HeaderAllowlistType", allowlist_type);
    }
  }
}

base::Optional<mojom::blink::FeaturePolicyFeature>
ParsingContext::ParseFeatureName(const String& feature_name) {
  DCHECK(!feature_name.IsEmpty());
  if (!feature_names_.Contains(feature_name)) {
    logger_.Warn("Unrecognized feature: '" + feature_name + "'.");
    return base::nullopt;
  }
  if (DisabledByOriginTrial(feature_name, execution_context_)) {
    logger_.Warn("Origin trial controlled feature not enabled: '" +
                 feature_name + "'.");
    return base::nullopt;
  }
  mojom::blink::FeaturePolicyFeature feature = feature_names_.at(feature_name);

  return feature;
}

ParsingContext::ParsedAllowlist ParsingContext::ParseAllowlist(
    const Vector<String>& origin_strings) {
  ParsedAllowlist allowlist;
  if (origin_strings.IsEmpty()) {
    // If a policy entry has no listed origins (e.g. "feature_name1" in
    // allow="feature_name1; feature_name2 value"), enable the feature for:
    //     a. |self_origin|, if we are parsing a header policy (i.e.,
    //       |src_origin| is null);
    //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
    //       |src_origin| is not null), |src_origin| is not opaque; or
    //     c. the opaque origin of the frame, if |src_origin| is opaque.
    if (!src_origin_) {
      allowlist.allowed_origins.push_back(self_origin_->ToUrlOrigin());
    } else if (!src_origin_->IsOpaque()) {
      allowlist.allowed_origins.push_back(src_origin_->ToUrlOrigin());
    } else {
      allowlist.matches_opaque_src = true;
    }
  } else {
    for (const String& origin_string : origin_strings) {
      DCHECK(!origin_string.IsEmpty());

      if (!origin_string.ContainsOnlyASCIIOrEmpty()) {
        logger_.Warn("Non-ASCII characters in origin.");
        continue;
      }

      // Determine the target of the declaration. This may be a specific
      // origin, either explicitly written, or one of the special keywords
      // 'self' or 'src'. ('src' can only be used in the iframe allow
      // attribute.)
      url::Origin target_origin;

      // If the iframe will have an opaque origin (for example, if it is
      // sandboxed, or has a data: URL), then 'src' needs to refer to the
      // opaque origin of the frame, which is not known yet. In this case,
      // the |matches_opaque_src| flag on the declaration is set, rather than
      // adding an origin to the allowlist.
      bool target_is_opaque = false;
      bool target_is_all = false;

      // 'self' origin is used if the origin is exactly 'self'.
      if (EqualIgnoringASCIICase(origin_string, "'self'")) {
        allowlist_includes_self_ = true;
        target_origin = self_origin_->ToUrlOrigin();
      }
      // 'src' origin is used if |src_origin| is available and the
      // origin is a match for 'src'. |src_origin| is only set
      // when parsing an iframe allow attribute.
      else if (src_origin_ && EqualIgnoringASCIICase(origin_string, "'src'")) {
        allowlist_includes_src_ = true;
        if (!src_origin_->IsOpaque()) {
          target_origin = src_origin_->ToUrlOrigin();
        } else {
          target_is_opaque = true;
        }
      } else if (EqualIgnoringASCIICase(origin_string, "'none'")) {
        allowlist_includes_none_ = true;
        continue;
      } else if (origin_string == "*") {
        allowlist_includes_star_ = true;
        target_is_all = true;
      }
      // Otherwise, parse the origin string and verify that the result is
      // valid. Invalid strings will produce an opaque origin, which will
      // result in an error message.
      else {
        scoped_refptr<SecurityOrigin> parsed_origin =
            SecurityOrigin::CreateFromString(origin_string);
        if (!parsed_origin->IsOpaque()) {
          target_origin = parsed_origin->ToUrlOrigin();
          allowlist_includes_origin_ = true;
        } else {
          logger_.Warn("Unrecognized origin: '" + origin_string + "'.");
          continue;
        }
      }

      if (target_is_all) {
        allowlist.matches_all_origins = true;
        allowlist.matches_opaque_src = true;
      } else if (target_is_opaque) {
        allowlist.matches_opaque_src = true;
      } else {
        allowlist.allowed_origins.push_back(target_origin);
      }
    }
  }

  // Size reduction: remove all items in the allowlist if target is all.
  if (allowlist.matches_all_origins)
    allowlist.allowed_origins.clear();

  // Sort |allowed_origins| in alphabetical order.
  std::sort(allowlist.allowed_origins.begin(), allowlist.allowed_origins.end());

  RecordAllowlistTypeUsage(origin_strings.size());

  return allowlist;
}

base::Optional<ParsedFeaturePolicyDeclaration> ParsingContext::ParseFeature(
    const FeaturePolicyDeclarationNode& declaration_node) {
  base::Optional<mojom::blink::FeaturePolicyFeature> feature =
      ParseFeatureName(declaration_node.feature_name);
  if (!feature)
    return base::nullopt;

  ParsedAllowlist parsed_allowlist = ParseAllowlist(declaration_node.allowlist);

  // If same feature appeared more than once, only the first one counts.
  if (feature_observer_.FeatureObserved(*feature))
    return base::nullopt;

  ParsedFeaturePolicyDeclaration parsed_feature(*feature);
  parsed_feature.allowed_origins = std::move(parsed_allowlist.allowed_origins);
  parsed_feature.matches_all_origins = parsed_allowlist.matches_all_origins;
  parsed_feature.matches_opaque_src = parsed_allowlist.matches_opaque_src;

  return parsed_feature;
}

ParsedFeaturePolicy ParsingContext::ParseFeaturePolicy(const String& policy) {
  return ParseIR(ParseFeaturePolicyToIR(policy));
}

ParsedFeaturePolicy ParsingContext::ParsePermissionsPolicy(
    const String& policy) {
  return ParseIR(ParsePermissionsPolicyToIR(policy));
}

ParsedFeaturePolicy ParsingContext::ParseIR(
    const ParsingContext::FeaturePolicyNode& root) {
  ParsedFeaturePolicy parsed_policy;
  for (const ParsingContext::FeaturePolicyDeclarationNode& declaration_node :
       root) {
    base::Optional<ParsedFeaturePolicyDeclaration> parsed_feature =
        ParseFeature(declaration_node);
    if (parsed_feature) {
      ReportFeatureUsage(parsed_feature->feature);
      parsed_policy.push_back(*parsed_feature);
    }
  }
  ReportAllowlistTypeUsage();
  return parsed_policy;
}

ParsingContext::FeaturePolicyNode ParsingContext::ParseFeaturePolicyToIR(
    const String& policy) {
  ParsingContext::FeaturePolicyNode root;

  if (policy.length() > MAX_LENGTH_PARSE) {
    logger_.Error("Feature policy declaration exceeds size limit(" +
                  String::Number(policy.length()) + ">" +
                  String::Number(MAX_LENGTH_PARSE) + ")");
    return {};
  }

  // RFC2616, section 4.2 specifies that headers appearing multiple times can be
  // combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  Vector<String> policy_items;
  // policy_items = [ policy *( "," [ policy ] ) ]
  policy.Split(',', policy_items);

  if (policy_items.size() > 1) {
    UseCounter::Count(
        execution_context_,
        mojom::blink::WebFeature::kFeaturePolicyCommaSeparatedDeclarations);
  }

  for (const String& item : policy_items) {
    Vector<String> feature_entries;
    // feature_entries = [ feature_entry *( ";" [ feature_entry ] ) ]
    item.Split(';', feature_entries);

    if (feature_entries.size() > 1) {
      UseCounter::Count(execution_context_,
                        mojom::blink::WebFeature::
                            kFeaturePolicySemicolonSeparatedDeclarations);
    }

    for (const String& feature_entry : feature_entries) {
      Vector<String> tokens;
      feature_entry.Split(' ', tokens);

      if (tokens.IsEmpty())
        continue;

      ParsingContext::FeaturePolicyDeclarationNode declaration_node;
      // Break tokens into head & tail, where
      // head = feature_name
      // tail = allowlist
      // After feature_name has been set, take tail of tokens vector by
      // erasing the first element.
      declaration_node.feature_name = std::move(tokens.front());
      tokens.erase(tokens.begin());
      declaration_node.allowlist = std::move(tokens);
      root.push_back(declaration_node);
    }
  }

  return root;
}

ParsingContext::FeaturePolicyNode ParsingContext::ParsePermissionsPolicyToIR(
    const String& policy) {
  if (policy.length() > MAX_LENGTH_PARSE) {
    logger_.Error("Permissions policy declaration exceeds size limit(" +
                  String::Number(policy.length()) + ">" +
                  String::Number(MAX_LENGTH_PARSE) + ")");
    return {};
  }

  auto root = net::structured_headers::ParseDictionary(policy.Utf8());
  if (!root) {
    logger_.Error(
        "Parse of permission policy failed because of errors reported by "
        "strctured header parser.");
    return {};
  }

  ParsingContext::FeaturePolicyNode ir_root;
  for (const auto& feature_entry : root.value()) {
    const auto& key = feature_entry.first;
    const char* feature_name = key.c_str();
    const auto& value = feature_entry.second;

    if (!value.params.empty()) {
      logger_.Warn(
          String::Format("Feature %s's parameters are ignored.", feature_name));
    }

    Vector<String> allowlist;
    for (const auto& parameterized_item : value.member) {
      if (!parameterized_item.params.empty()) {
        logger_.Warn(String::Format("Feature %s's parameters are ignored.",
                                    feature_name));
      }

      String allowlist_item;
      if (parameterized_item.item.is_token()) {
        // All special keyword appears as token, i.e. self, src and *.
        const std::string& token_value = parameterized_item.item.GetString();
        if (token_value != "*" && token_value != "self") {
          logger_.Warn(String::Format(
              "Invalid allowlist item(%s) for feature %s. Allowlist item "
              "must be *, self or quoted url.",
              token_value.c_str(), feature_name));
          continue;
        }

        if (token_value == "*") {
          allowlist_item = "*";
        } else {
          allowlist_item = String::Format("'%s'", token_value.c_str());
        }
      } else if (parameterized_item.item.is_string()) {
        allowlist_item = parameterized_item.item.GetString().c_str();
      } else {
        logger_.Warn(
            String::Format("Invalid allowlist item for feature %s. Allowlist "
                           "item must be *, self, or quoted url.",
                           feature_name));
        continue;
      }
      if (!allowlist_item.IsEmpty())
        allowlist.push_back(allowlist_item);
    }

    if (allowlist.IsEmpty())
      allowlist.push_back("'none'");

    ir_root.push_back(
        ParsingContext::FeaturePolicyDeclarationNode{feature_name, allowlist});
  }

  return ir_root;
}

}  // namespace

ParsedFeaturePolicy FeaturePolicyParser::ParseHeader(
    const String& feature_policy_header,
    const String& permissions_policy_header,
    scoped_refptr<const SecurityOrigin> origin,
    PolicyParserMessageBuffer& feature_policy_logger,
    PolicyParserMessageBuffer& permissions_policy_logger,
    ExecutionContext* execution_context) {
  ParsedFeaturePolicy permissions_policy =
      ParsingContext(permissions_policy_logger, origin, nullptr,
                     GetDefaultFeatureNameMap(), execution_context)
          .ParsePermissionsPolicy(permissions_policy_header);
  ParsedFeaturePolicy feature_policy =
      ParsingContext(feature_policy_logger, origin, nullptr,
                     GetDefaultFeatureNameMap(), execution_context)
          .ParseFeaturePolicy(feature_policy_header);

  FeatureObserver observer;
  for (const auto& policy_declaration : permissions_policy) {
    bool feature_observed =
        observer.FeatureObserved(policy_declaration.feature);
    DCHECK(!feature_observed);
  }
  for (const auto& policy_declaration : feature_policy) {
    if (!observer.FeatureObserved(policy_declaration.feature)) {
      permissions_policy.push_back(policy_declaration);
    } else {
      feature_policy_logger.Warn(String::Format(
          "Feature %s has been specified in both Feature-Policy and "
          "Permissions-Policy header. Value defined in Permissions-Policy "
          "header will be used.",
          GetNameForFeature(policy_declaration.feature).Ascii().c_str()));
    }
  }

  return permissions_policy;
}

ParsedFeaturePolicy FeaturePolicyParser::ParseAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    PolicyParserMessageBuffer& logger,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, self_origin, src_origin,
                        GetDefaultFeatureNameMap(), execution_context)
      .ParseFeaturePolicy(policy);
}

ParsedFeaturePolicy FeaturePolicyParser::ParseFeaturePolicyForTest(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    PolicyParserMessageBuffer& logger,
    const FeatureNameMap& feature_names,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, self_origin, src_origin, feature_names,
                        execution_context)
      .ParseFeaturePolicy(policy);
}

ParsedFeaturePolicy FeaturePolicyParser::ParsePermissionsPolicyForTest(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    PolicyParserMessageBuffer& logger,
    const FeatureNameMap& feature_names,
    ExecutionContext* execution_context) {
  return ParsingContext(logger, self_origin, src_origin, feature_names,
                        execution_context)
      .ParsePermissionsPolicy(policy);
}

bool IsFeatureDeclared(mojom::blink::FeaturePolicyFeature feature,
                       const ParsedFeaturePolicy& policy) {
  return std::any_of(policy.begin(), policy.end(),
                     [feature](const auto& declaration) {
                       return declaration.feature == feature;
                     });
}

bool RemoveFeatureIfPresent(mojom::blink::FeaturePolicyFeature feature,
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

bool DisallowFeatureIfNotPresent(mojom::blink::FeaturePolicyFeature feature,
                                 ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedFeaturePolicyDeclaration allowlist(feature);
  policy.push_back(allowlist);
  return true;
}

bool AllowFeatureEverywhereIfNotPresent(
    mojom::blink::FeaturePolicyFeature feature,
    ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedFeaturePolicyDeclaration allowlist(feature);
  allowlist.matches_all_origins = true;
  allowlist.matches_opaque_src = true;
  policy.push_back(allowlist);
  return true;
}

void DisallowFeature(mojom::blink::FeaturePolicyFeature feature,
                     ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  DisallowFeatureIfNotPresent(feature, policy);
}

bool IsFeatureForMeasurementOnly(mojom::blink::FeaturePolicyFeature feature) {
  return feature == mojom::blink::FeaturePolicyFeature::kWebShare;
}

void AllowFeatureEverywhere(mojom::blink::FeaturePolicyFeature feature,
                            ParsedFeaturePolicy& policy) {
  RemoveFeatureIfPresent(feature, policy);
  AllowFeatureEverywhereIfNotPresent(feature, policy);
}

const Vector<String> GetAvailableFeatures(ExecutionContext* execution_context) {
  Vector<String> available_features;
  for (const auto& feature : GetDefaultFeatureNameMap()) {
    if (!DisabledByOriginTrial(feature.key, execution_context) &&
        !IsFeatureForMeasurementOnly(feature.value)) {
      available_features.push_back(feature.key);
    }
  }
  return available_features;
}

const String& GetNameForFeature(mojom::blink::FeaturePolicyFeature feature) {
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (entry.value == feature)
      return entry.key;
  }
  return g_empty_string;
}

}  // namespace blink
