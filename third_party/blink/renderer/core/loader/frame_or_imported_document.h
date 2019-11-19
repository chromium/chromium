// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_OR_IMPORTED_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_OR_IMPORTED_DOCUMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;
class DocumentLoader;
class LocalFrame;

// FrameOrImportedDocument is a thin wrapper that represents and absorbs
// the differences between Document-ish things behind FrameFetchContext:
// - An imported Document
//   (document_loader_ is null, document_ is non-null),
// - A DocumentLoader that has committed its navigation
//   (document_loader_ is non-null, document_ is non-null).
//
// A FrameOrImportedDocument is indirectly used with a ResourceFetcher, and
// works until ResourceFetcher::ClearContext is called. Do not use this class
// outside of core/loader.
class CORE_EXPORT FrameOrImportedDocument final
    : public GarbageCollected<FrameOrImportedDocument> {
 public:
  // Creates a FrameOrImportedDocument associated with a LocalFrame (which is,
  // loader.GetFrame() which cannot be null). This object works until the
  // associated frame is detached.
  FrameOrImportedDocument(DocumentLoader& loader, Document& committed_document)
      : document_loader_(loader), document_(committed_document) {}

  // Creates a FrameOrImportedDocument associated with an imported document.
  // This object works until |document| is shut-down.
  explicit FrameOrImportedDocument(Document& imported_document)
      : document_(imported_document) {}

  // When this object is associated with a frame, returns it.
  // When this object is associated with an imported document, returns the
  // frame associated with the imports controller.
  LocalFrame& GetFrame() const;

  // When this object is associated with a frame, returns the document loader
  // given to the constructor (note that there may be multiple document loaders
  // in a LocalFrame).
  // When this object is associated with an imported document, returns the
  // document loader associated with the frame associated with the imports
  // controller.
  DocumentLoader& GetMasterDocumentLoader() const;

  // When this object is associated with a frame, returns the document loader
  // given to the constructor (note that there may be multiple document loaders
  // in a LocalFrame). Otherwise, returns null.
  DocumentLoader* GetDocumentLoader() const { return document_loader_; }

  // When this object is associated with a frame with a committed document,
  // returns it.
  // When this object is associated with an imported document, returns it.
  Document& GetDocument() const { return *document_; }

  void Trace(Visitor* visitor);

 private:
  const Member<DocumentLoader> document_loader_;
  const Member<Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FRAME_OR_IMPORTED_DOCUMENT_H_
