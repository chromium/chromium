// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;

class MODULES_EXPORT Credential : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~Credential() override;
  void Trace(Visitor*) const override;

  virtual bool IsPasswordCredential() const { return false; }
  virtual bool IsFederatedCredential() const { return false; }
  virtual bool IsPublicKeyCredential() const { return false; }
  virtual bool IsOTPCredential() const { return false; }
  virtual bool IsPaymentCredential() const { return false; }

  // Credential.idl
  const String& id() const { return id_; }
  const String& type() const { return type_; }

 protected:
  Credential(const String& id, const String& type);

  // Parses a String into a KURL that is potentially empty or null. Throws an
  // exception via |exceptionState| if an invalid URL is produced.
  static KURL ParseStringAsURLOrThrow(const String&, ExceptionState&);

 private:
  String id_;
  String type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGER_CREDENTIAL_H_
