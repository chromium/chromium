// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_PENDING_IMPORT_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_PENDING_IMPORT_MAP_H_

#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class Document;
class ImportMap;
class KURL;
class ScriptElementBase;
class ScriptValue;

// PendingImportMap serves as a container for an import map after "prepare a
// script" until it is registered. PendingImportMap is similar to PendingScript.
//
// After PendingImportMap is ready, PendingImportMap works mostly as
// https://wicg.github.io/import-maps/#import-map-parse-result and
// |element_|'s script's result is |this|,
// except for "null import map parse result" corresponds to
// non-null PendingImportMap with |import_map_| == nullptr.
//
// Note: Currently we only support inline import maps and PendingImportMap is
// always ready.
class CORE_EXPORT PendingImportMap final
    : public GarbageCollected<PendingImportMap> {
 public:
  // https://wicg.github.io/import-maps/#create-an-import-map-parse-result
  // for inline import maps.
  static PendingImportMap* CreateInline(ScriptElementBase&,
                                        const String& import_map_text,
                                        const KURL& base_url);

  PendingImportMap(ScriptState* script_state,
                   ScriptElementBase&,
                   ImportMap*,
                   ScriptValue error_to_rethrow,
                   const Document& original_context_document);

  void RegisterImportMap() const;

  virtual void Trace(Visitor* visitor);

 private:
  Member<ScriptElementBase> element_;

  // https://wicg.github.io/import-maps/#import-map-parse-result-import-map
  Member<ImportMap> import_map_;

  // https://wicg.github.io/import-maps/#import-map-parse-result-error-to-rethrow
  // The error is TypeError if the string is non-null, or null otherwise.
  WorldSafeV8Reference<v8::Value> error_to_rethrow_;

  // https://wicg.github.io/import-maps/#import-map-parse-result-settings-object
  // The context document at the time when PrepareScript() is executed.
  // This is only used to check whether the script element is moved between
  // documents and thus doesn't retain a strong reference.
  WeakMember<const Document> original_context_document_;

  DISALLOW_COPY_AND_ASSIGN(PendingImportMap);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_PENDING_IMPORT_MAP_H_
