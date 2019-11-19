// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_TREE_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_IMPORTS_HTML_IMPORT_TREE_ROOT_H_

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/html/imports/html_import.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class HTMLImportChild;
class KURL;

class HTMLImportTreeRoot final : public HTMLImport, public NameClient {
 public:
  explicit HTMLImportTreeRoot(Document*);
  ~HTMLImportTreeRoot() final;
  void Dispose();

  // HTMLImport overrides:
  Document* GetDocument() const final;
  bool HasFinishedLoading() const final;
  void StateWillChange() final;
  void StateDidChange() final;

  void ScheduleRecalcState();

  HTMLImportChild* Add(HTMLImportChild*);
  HTMLImportChild* Find(const KURL&) const;

  void Trace(Visitor*) override;
  const char* NameInHeapSnapshot() const override {
    return "HTMLImportTreeRoot";
  }

 private:
  void RecalcTimerFired(TimerBase*);

  Member<Document> document_;
  TaskRunnerTimer<HTMLImportTreeRoot> recalc_timer_;

  // List of import which has been loaded or being loaded.
  typedef HeapVector<Member<HTMLImportChild>> ImportList;
  ImportList imports_;
};

DEFINE_TYPE_CASTS(HTMLImportTreeRoot,
                  HTMLImport,
                  import,
                  import->IsRoot(),
                  import.IsRoot());

}  // namespace blink

#endif
