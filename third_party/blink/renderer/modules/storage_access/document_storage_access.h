// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_ACCESS_DOCUMENT_STORAGE_ACCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_ACCESS_DOCUMENT_STORAGE_ACCESS_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Document;
class ScriptPromise;
class ScriptState;
class StorageAccessTypes;

class DocumentStorageAccess final
    : public GarbageCollected<DocumentStorageAccess>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static const char kNoAccessRequested[];
  static DocumentStorageAccess& From(Document& document);
  static ScriptPromise requestStorageAccess(
      ScriptState* script_state,
      Document& document,
      const StorageAccessTypes* storage_access_types);
  static ScriptPromise hasUnpartitionedCookieAccess(ScriptState* script_state,
                                                    Document& document);

  explicit DocumentStorageAccess(Document& document);
  void Trace(Visitor*) const override;

  ScriptPromise requestStorageAccess(
      ScriptState* script_state,
      const StorageAccessTypes* storage_access_types);
  ScriptPromise hasUnpartitionedCookieAccess(ScriptState* script_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_STORAGE_ACCESS_DOCUMENT_STORAGE_ACCESS_H_
