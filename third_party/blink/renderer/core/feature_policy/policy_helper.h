// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_POLICY_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_POLICY_HELPER_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/document_policy_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class PolicyParserMessageBuffer {
 public:
  struct Message {
    mojom::blink::ConsoleMessageLevel level;
    String content;

    Message(mojom::blink::ConsoleMessageLevel level, const String& content)
        : level(level), content(content) {}
  };

  PolicyParserMessageBuffer() = default;
  explicit PolicyParserMessageBuffer(const String& prefix,
                                     bool discard_message = false)
      : prefix_(prefix), discard_message_(discard_message) {}

  ~PolicyParserMessageBuffer() = default;

  void Warn(const String& message) {
    if (!discard_message_) {
      message_buffer_.emplace_back(mojom::blink::ConsoleMessageLevel::kWarning,
                                   prefix_ + message);
    }
  }

  void Error(const String& message) {
    if (!discard_message_) {
      message_buffer_.emplace_back(mojom::blink::ConsoleMessageLevel::kError,
                                   prefix_ + message);
    }
  }

  const Vector<Message>& GetMessages() { return message_buffer_; }

 private:
  String prefix_;
  Vector<Message> message_buffer_;
  // If a dummy message buffer is desired, i.e. messages are not needed for
  // the caller, this flag can be set to true and the message buffer will
  // discard any incoming messages.
  bool discard_message_ = false;
};

using FeatureNameMap = HashMap<String, mojom::blink::FeaturePolicyFeature>;

using DocumentPolicyFeatureSet = HashSet<
    mojom::blink::DocumentPolicyFeature,
    DefaultHash<mojom::blink::DocumentPolicyFeature>::Hash,
    WTF::EnumOrGenericHashTraits<true, mojom::blink::DocumentPolicyFeature>>;

class FeatureContext;

// This method defines the feature names which will be recognized by the parser
// for the Feature-Policy HTTP header and the <iframe> "allow" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
const FeatureNameMap& GetDefaultFeatureNameMap();

// This method defines the feature names which will be recognized by the parser
// for the Document-Policy HTTP header and the <iframe> "policy" attribute, as
// well as the features which will be recognized by the document or iframe
// policy object.
const DocumentPolicyFeatureSet& GetAvailableDocumentPolicyFeatures();

// Refresh the set content based on current RuntimeFeatures environment.
CORE_EXPORT void ResetAvailableDocumentPolicyFeaturesForTest();

// Returns true if this FeaturePolicyFeature is currently disabled by an origin
// trial (it is origin trial controlled, and the origin trial is not enabled).
// The first String param should be a name of FeaturePolicyFeature.
bool DisabledByOriginTrial(const String&, FeatureContext*);

// Returns true if this DocumentPolicyFeature is currently disabled by an origin
// trial (it is origin trial controlled, and the origin trial is not enabled).
bool DisabledByOriginTrial(mojom::blink::DocumentPolicyFeature,
                           FeatureContext*);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_POLICY_HELPER_H_
