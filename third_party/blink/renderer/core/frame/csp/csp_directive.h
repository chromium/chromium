// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContentSecurityPolicy;

class CORE_EXPORT CSPDirective : public GarbageCollected<CSPDirective> {
 public:
  CSPDirective(const String& name,
               const String& value,
               ContentSecurityPolicy* policy)
      : name_(name), text_(name + ' ' + value), policy_(policy) {}
  virtual ~CSPDirective() = default;
  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(policy_); }

  const String& GetName() const { return name_; }
  const String& GetText() const { return text_; }

 protected:
  ContentSecurityPolicy* Policy() const { return policy_; }

 private:
  String name_;
  String text_;
  Member<ContentSecurityPolicy> policy_;

  DISALLOW_COPY_AND_ASSIGN(CSPDirective);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_DIRECTIVE_H_
