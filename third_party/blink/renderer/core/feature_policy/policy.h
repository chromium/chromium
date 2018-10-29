// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_POLICY_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class SecurityOrigin;

// Policy provides an interface for feature policy introspection of a document
// (DocumentPolicy) or an iframe (IFramePolicy).
class CORE_EXPORT Policy : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~Policy() override = default;

  // Implementation of methods of the policy interface:
  // Returns whether or not the given feature is allowed on the origin of the
  // document that owns the policy.
  bool allowsFeature(const String& feature) const;
  // Returns whether or not the given feature is allowed on the origin of the
  // given URL.
  bool allowsFeature(const String& feature, const String& url) const;
  // Returns a list of feature names that are allowed on the self origin.
  Vector<String> allowedFeatures() const;
  // Returns a list of feature name that are allowed on the origin of the given
  // URL.
  Vector<String> getAllowlistForFeature(const String& url) const;

  // Inform the Policy object when the container policy on its frame element has
  // changed.
  virtual void UpdateContainerPolicy(
      const ParsedFeaturePolicy& container_policy = {},
      scoped_refptr<const SecurityOrigin> src_origin = nullptr);

  void Trace(blink::Visitor*) override;

 protected:
  virtual const FeaturePolicy* GetPolicy() const = 0;
  // Get the containing document.
  virtual Document* GetDocument() const = 0;

 private:
  // Add console message to the containing document.
  void AddWarningForUnrecognizedFeature(const String& message) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FEATURE_POLICY_POLICY_H_
