// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIAL_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIAL_METRICS_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class ScriptState;

// Helper class for CredentialsContainer API method implementations to record
// metrics, and maintain metric-related state. It is unique per Document.
class CredentialMetrics : public GarbageCollected<CredentialMetrics>,
                          public GarbageCollectedMixin {
 public:
  static CredentialMetrics& From(ScriptState* script_state);

  explicit CredentialMetrics(Document& frame);
  virtual ~CredentialMetrics();

  CredentialMetrics(const CredentialMetrics&) = delete;
  CredentialMetrics& operator=(const CredentialMetrics&) = delete;

  void RecordWebAuthnConditionalUiCall();

  void Trace(Visitor* visitor) const override { visitor->Trace(document_); }

 private:
  Member<Document> document_;

  bool conditional_ui_timing_reported_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_CREDENTIAL_METRICS_H_
