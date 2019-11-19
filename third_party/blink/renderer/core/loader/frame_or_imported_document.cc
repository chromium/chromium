// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"

namespace blink {

LocalFrame& FrameOrImportedDocument::GetFrame() const {
  DCHECK(document_);
  if (document_loader_) {
    LocalFrame* frame = document_->GetFrame();
    DCHECK(frame);
    return *frame;
  }

  // HTML imports case
  // It's guaranteed that imports_controller is not nullptr since:
  // - only ClearImportsController() clears it
  // - ClearImportsController() also calls ResourceFethcer::ClearContext().
  HTMLImportsController* imports_controller = document_->ImportsController();
  DCHECK(imports_controller);

  // It's guaranteed that Master() is not yet Shutdown()-ed since when Master()
  // is Shutdown()-ed:
  // - Master()'s HTMLImportsController is disposed.
  // - All the HTMLImportLoader instances of the HTMLImportsController are
  //   disposed.
  // - ClearImportsController() is called on the Document.
  // HTMLImportsController is created only when the master Document's
  // GetFrame() doesn't return nullptr, this is guaranteed to be not nullptr
  // here.
  LocalFrame* frame = imports_controller->Master()->GetFrame();
  DCHECK(frame);
  return *frame;
}

DocumentLoader& FrameOrImportedDocument::GetMasterDocumentLoader() const {
  if (document_loader_)
    return *document_loader_;

  // HTML imports case
  // GetDocumentLoader() here always returns a non-nullptr value that is the
  // DocumentLoader for |document_| because:
  // - A Document is created with a LocalFrame only after the
  //   DocumentLoader is committed
  // - When another DocumentLoader is committed, the FrameLoader
  //   Shutdown()-s |document_|.
  auto* loader = GetFrame().Loader().GetDocumentLoader();
  DCHECK(loader);
  return *loader;
}

void FrameOrImportedDocument::Trace(Visitor* visitor) {
  visitor->Trace(document_loader_);
  visitor->Trace(document_);
}

}  // namespace blink
