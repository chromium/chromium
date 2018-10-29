// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/bit_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/origin.h"

namespace blink {

ParsedFeaturePolicy ParseFeaturePolicyHeader(
    const String& policy,
    scoped_refptr<const SecurityOrigin> origin,
    Vector<String>* messages) {
  return ParseFeaturePolicy(policy, origin, nullptr, messages,
                            GetDefaultFeatureNameMap());
}

ParsedFeaturePolicy ParseFeaturePolicyAttribute(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    Document* document) {
  return ParseFeaturePolicy(policy, self_origin, src_origin, messages,
                            GetDefaultFeatureNameMap(), document);
}

ParsedFeaturePolicy ParseFeaturePolicy(
    const String& policy,
    scoped_refptr<const SecurityOrigin> self_origin,
    scoped_refptr<const SecurityOrigin> src_origin,
    Vector<String>* messages,
    const FeatureNameMap& feature_names,
    Document* document) {
  ParsedFeaturePolicy allowlists;
  BitVector features_specified(
      static_cast<int>(mojom::FeaturePolicyFeature::kMaxValue));

  // RFC2616, section 4.2 specifies that headers appearing multiple times can be
  // combined with a comma. Walk the header string, and parse each comma
  // separated chunk as a separate header.
  Vector<String> policy_items;
  // policy_items = [ policy *( "," [ policy ] ) ]
  policy.Split(',', policy_items);
  for (const String& item : policy_items) {
    Vector<String> entry_list;
    // entry_list = [ entry *( ";" [ entry ] ) ]
    item.Split(';', entry_list);
    for (const String& entry : entry_list) {
      // Split removes extra whitespaces by default
      //     "name value1 value2" or "name".
      Vector<String> tokens;
      entry.Split(' ', tokens);
      // Empty policy. Skip.
      if (tokens.IsEmpty())
        continue;
      if (!feature_names.Contains(tokens[0])) {
        if (messages)
          messages->push_back("Unrecognized feature: '" + tokens[0] + "'.");
        continue;
      }

      mojom::FeaturePolicyFeature feature = feature_names.at(tokens[0]);
      // If a policy has already been specified for the current feature, drop
      // the new policy.
      if (features_specified.QuickGet(static_cast<int>(feature)))
        continue;

      // Count the use of this feature policy.
      if (src_origin) {
        if (!document || !document->IsParsedFeaturePolicy(feature)) {
          UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Allow",
                                    feature);
          if (document) {
            document->SetParsedFeaturePolicy(feature);
          }
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.FeaturePolicy.Header",
                                  feature);
      }

      ParsedFeaturePolicyDeclaration allowlist;
      allowlist.feature = feature;
      features_specified.QuickSet(static_cast<int>(feature));
      std::vector<url::Origin> origins;
      // If a policy entry has no (optional) values (e,g,
      // allow="feature_name1; feature_name2 value"), enable the feature for:
      //     a. |self_origin|, if we are parsing a header policy (i.e.,
      //       |src_origin| is null);
      //     b. |src_origin|, if we are parsing an allow attribute (i.e.,
      //       |src_origin| is not null), |src_origin| is not opaque; or
      //     c. the opaque origin of the frame, if |src_origin| is opaque.
      if (tokens.size() == 1) {
        if (!src_origin) {
          origins.push_back(self_origin->ToUrlOrigin());
        } else if (!src_origin->IsOpaque()) {
          origins.push_back(src_origin->ToUrlOrigin());
        } else {
          allowlist.matches_opaque_src = true;
        }
      }

      for (wtf_size_t i = 1; i < tokens.size(); i++) {
        if (!tokens[i].ContainsOnlyASCIIOrEmpty()) {
          messages->push_back("Non-ASCII characters in origin.");
          continue;
        }
        if (EqualIgnoringASCIICase(tokens[i], "'self'")) {
          origins.push_back(self_origin->ToUrlOrigin());
        } else if (src_origin && EqualIgnoringASCIICase(tokens[i], "'src'")) {
          // Only the iframe allow attribute can define |src_origin|.
          // When parsing feature policy header, 'src' is disallowed and
          // |src_origin| = nullptr.
          // If the iframe will have an opaque origin (for example, if it is
          // sandboxed, or has a data: URL), then 'src' needs to refer to the
          // opaque origin of the frame, which is not known yet. In this case,
          // the |matches_opaque_src| flag on the declaration is set, rather
          // than adding an origin to the allowlist.
          if (src_origin->IsOpaque()) {
            allowlist.matches_opaque_src = true;
          } else {
            origins.push_back(src_origin->ToUrlOrigin());
          }
        } else if (EqualIgnoringASCIICase(tokens[i], "'none'")) {
          continue;
        } else if (tokens[i] == "*") {
          allowlist.matches_all_origins = true;
          break;
        } else {
          scoped_refptr<SecurityOrigin> target_origin =
              SecurityOrigin::CreateFromString(tokens[i]);
          if (!target_origin->IsOpaque())
            origins.push_back(target_origin->ToUrlOrigin());
          else if (messages)
            messages->push_back("Unrecognized origin: '" + tokens[i] + "'.");
        }
      }
      allowlist.origins = origins;
      allowlists.push_back(allowlist);
    }
  }
  return allowlists;
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
  ParsedFeaturePolicyDeclaration allowlist;
  allowlist.feature = feature;
  allowlist.matches_all_origins = false;
  allowlist.matches_opaque_src = false;
  policy.push_back(allowlist);
  return true;
}

bool AllowFeatureEverywhereIfNotPresent(mojom::FeaturePolicyFeature feature,
                                        ParsedFeaturePolicy& policy) {
  if (IsFeatureDeclared(feature, policy))
    return false;
  ParsedFeaturePolicyDeclaration allowlist;
  allowlist.feature = feature;
  allowlist.matches_all_origins = true;
  allowlist.matches_opaque_src = true;
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

// This method defines the feature names which will be recognized by the parser
// for the Feature-Policy HTTP header and the <iframe> "allow" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
//
// Features which are implemented behind a flag should generally also have the
// same flag controlling whether they are in this map. Note that features which
// are shipping as part of an origin trial should add their feature names to
// this map unconditionally, as the trial token could be added after the HTTP
// header needs to be parsed. This also means that top-level documents which
// simply want to embed another page which uses an origin trial feature, without
// using the feature themselves, can use feature policy to allow use of the
// feature in subframes. (The framed document will still require a valid origin
// trial token to use the feature in this scenario.)
const FeatureNameMap& GetDefaultFeatureNameMap() {
  DEFINE_STATIC_LOCAL(FeatureNameMap, default_feature_name_map, ());
  if (default_feature_name_map.IsEmpty()) {
    default_feature_name_map.Set("autoplay",
                                 mojom::FeaturePolicyFeature::kAutoplay);
    default_feature_name_map.Set("camera",
                                 mojom::FeaturePolicyFeature::kCamera);
    default_feature_name_map.Set("encrypted-media",
                                 mojom::FeaturePolicyFeature::kEncryptedMedia);
    default_feature_name_map.Set("fullscreen",
                                 mojom::FeaturePolicyFeature::kFullscreen);
    default_feature_name_map.Set("geolocation",
                                 mojom::FeaturePolicyFeature::kGeolocation);
    default_feature_name_map.Set("microphone",
                                 mojom::FeaturePolicyFeature::kMicrophone);
    default_feature_name_map.Set("midi",
                                 mojom::FeaturePolicyFeature::kMidiFeature);
    default_feature_name_map.Set("speaker",
                                 mojom::FeaturePolicyFeature::kSpeaker);
    default_feature_name_map.Set("sync-xhr",
                                 mojom::FeaturePolicyFeature::kSyncXHR);
    // Under origin trial: Should be made conditional on WebVR and WebXR
    // runtime flags once it is out of trial.
    default_feature_name_map.Set("vr", mojom::FeaturePolicyFeature::kWebVr);
    if (RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled()) {
      default_feature_name_map.Set(
          "layout-animations", mojom::FeaturePolicyFeature::kLayoutAnimations);
      default_feature_name_map.Set("document-write",
                                   mojom::FeaturePolicyFeature::kDocumentWrite);
      default_feature_name_map.Set(
          "image-compression", mojom::FeaturePolicyFeature::kImageCompression);
      default_feature_name_map.Set("lazyload",
                                   mojom::FeaturePolicyFeature::kLazyLoad);
      default_feature_name_map.Set(
          "legacy-image-formats",
          mojom::FeaturePolicyFeature::kLegacyImageFormats);
      default_feature_name_map.Set(
          "max-downscaling-image",
          mojom::FeaturePolicyFeature::kMaxDownscalingImage);
      default_feature_name_map.Set("unsized-media",
                                   mojom::FeaturePolicyFeature::kUnsizedMedia);
      default_feature_name_map.Set(
          "vertical-scroll", mojom::FeaturePolicyFeature::kVerticalScroll);
      default_feature_name_map.Set("sync-script",
                                   mojom::FeaturePolicyFeature::kSyncScript);
    }
    if (RuntimeEnabledFeatures::PaymentRequestEnabled()) {
      default_feature_name_map.Set("payment",
                                   mojom::FeaturePolicyFeature::kPayment);
    }
    if (RuntimeEnabledFeatures::PictureInPictureAPIEnabled()) {
      default_feature_name_map.Set(
          "picture-in-picture", mojom::FeaturePolicyFeature::kPictureInPicture);
    }
    if (RuntimeEnabledFeatures::SensorEnabled()) {
      default_feature_name_map.Set("accelerometer",
                                   mojom::FeaturePolicyFeature::kAccelerometer);
      default_feature_name_map.Set(
          "ambient-light-sensor",
          mojom::FeaturePolicyFeature::kAmbientLightSensor);
      default_feature_name_map.Set("gyroscope",
                                   mojom::FeaturePolicyFeature::kGyroscope);
      default_feature_name_map.Set("magnetometer",
                                   mojom::FeaturePolicyFeature::kMagnetometer);
    }
    if (RuntimeEnabledFeatures::WebUSBEnabled()) {
      default_feature_name_map.Set("usb", mojom::FeaturePolicyFeature::kUsb);
    }
  }
  return default_feature_name_map;
}

const String& GetNameForFeature(mojom::FeaturePolicyFeature feature) {
  for (const auto& entry : GetDefaultFeatureNameMap()) {
    if (entry.value == feature)
      return entry.key;
  }
  return g_empty_string;
}

}  // namespace blink
