// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_REQUEST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// An import assertion key/value pair per spec:
// https://tc39.es/proposal-import-assertions/
struct ImportAssertion {
  String key;
  String value;
  TextPosition position;
  ImportAssertion(const String& key,
                  const String& value,
                  const TextPosition& position)
      : key(key), value(value), position(position) {}
};

// An instance of a ModuleRequest record:
// https://tc39.es/proposal-import-assertions/#sec-modulerequest-record
// Represents a module script's request to import a module given a specifier and
// list of import assertions.
struct CORE_EXPORT ModuleRequest {
  String specifier;
  TextPosition position;
  Vector<ImportAssertion> import_assertions;
  ModuleRequest(const String& specifier,
                const TextPosition& position,
                const Vector<ImportAssertion>& import_assertions)
      : specifier(specifier),
        position(position),
        import_assertions(import_assertions) {}

  String GetModuleTypeString() const;

  bool HasInvalidImportAttributeKey(String* invalid_key) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_REQUEST_H_
