// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/document_metadata_server.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/document_metadata/document_metadata_extractor.h"

namespace blink {

// static
DocumentMetadataServer* DocumentMetadataServer::From(Document& document) {
  return document.GetDocumentMetadataServer();
}

// static
void DocumentMetadataServer::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::DocumentMetadata> receiver) {
  DCHECK(frame && frame->GetDocument());
  auto& document = *frame->GetDocument();
  auto* server = DocumentMetadataServer::From(document);
  if (!server) {
    server = MakeGarbageCollected<DocumentMetadataServer>(
        base::PassKey<DocumentMetadataServer>(), *frame);
    document.SetDocumentMetadataServer(server);
  }
  server->Bind(std::move(receiver));
}

DocumentMetadataServer::DocumentMetadataServer(
    base::PassKey<DocumentMetadataServer>,
    LocalFrame& frame)
    : document_(*frame.GetDocument()), receiver_(this, frame.DomWindow()) {}

void DocumentMetadataServer::Bind(
    mojo::PendingReceiver<mojom::blink::DocumentMetadata> receiver) {
  // We expect the interface to be bound at most once when the page is loaded
  // to service the GetEntities() call.
  receiver_.reset();
  // See https://bit.ly/2S0zRAS for task types.
  receiver_.Bind(std::move(receiver),
                 document_->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

void DocumentMetadataServer::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(receiver_);
}

void DocumentMetadataServer::GetEntities(GetEntitiesCallback callback) {
  std::move(callback).Run(DocumentMetadataExtractor::Extract(*document_));
}

}  // namespace blink
