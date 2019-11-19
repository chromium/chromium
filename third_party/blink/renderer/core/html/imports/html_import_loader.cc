/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"

#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_sync_microtask_queue.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"

namespace blink {

HTMLImportLoader::HTMLImportLoader(HTMLImportsController* controller)
    : controller_(controller),
      state_(kStateLoading),
      microtask_queue_(
          MakeGarbageCollected<V0CustomElementSyncMicrotaskQueue>()) {}

HTMLImportLoader::~HTMLImportLoader() = default;

void HTMLImportLoader::Dispose() {
  controller_ = nullptr;
  if (document_) {
    if (document_->Parser())
      document_->Parser()->RemoveClient(this);
    document_->ClearImportsController();
    document_.Clear();
  }
  ClearResource();
}

void HTMLImportLoader::ResponseReceived(Resource* resource,
                                        const ResourceResponse& response) {
  // Resource may already have been loaded with the import loader
  // being added as a client later & now being notified. Fail early.
  if (resource->LoadFailedOrCanceled() || response.HttpStatusCode() >= 400 ||
      !response.HttpHeaderField(http_names::kContentDisposition).IsNull()) {
    SetState(kStateError);
    return;
  }
  SetState(StartWritingAndParsing(response));
}

void HTMLImportLoader::DataReceived(Resource* resource,
                                    const char* data,
                                    size_t length) {
  document_->Parser()->AppendBytes(data, length);
}

void HTMLImportLoader::NotifyFinished(Resource* resource) {
  // If part of the document was already loaded, we don't treat the load failure
  // as an error because the partially-loaded  document has been visible from
  // script at this point.
  if (resource->LoadFailedOrCanceled() && !document_) {
    SetState(kStateError);
    return;
  }

  SetState(FinishWriting());
}

HTMLImportLoader::State HTMLImportLoader::StartWritingAndParsing(
    const ResourceResponse& response) {
  DCHECK(controller_);
  DCHECK(!imports_.IsEmpty());
  document_ = MakeGarbageCollected<HTMLDocument>(
      DocumentInit::CreateWithImportsController(controller_)
          .WithURL(response.CurrentRequestUrl()));
  document_->OpenForNavigation(kAllowAsynchronousParsing, response.MimeType(),
                               "UTF-8");

  DocumentParser* parser = document_->Parser();
  DCHECK(parser);
  parser->AddClient(this);

  return kStateLoading;
}

HTMLImportLoader::State HTMLImportLoader::FinishWriting() {
  return kStateWritten;
}

HTMLImportLoader::State HTMLImportLoader::FinishParsing() {
  return kStateParsed;
}

HTMLImportLoader::State HTMLImportLoader::FinishLoading() {
  return kStateLoaded;
}

void HTMLImportLoader::SetState(State state) {
  if (state_ == state)
    return;

  state_ = state;

  if (state_ == kStateParsed || state_ == kStateError ||
      state_ == kStateWritten) {
    if (document_)
      document_->Parser()->Finish();
  }

  // Since DocumentParser::Finish() can let setState() reenter, we shouldn't
  // refer to state_ here.
  if (state == kStateLoaded)
    document_->SetReadyState(Document::kComplete);
  if (state == kStateLoaded || state == kStateError)
    DidFinishLoading();
}

void HTMLImportLoader::NotifyParserStopped() {
  SetState(FinishParsing());
  if (!HasPendingResources())
    SetState(FinishLoading());

  DocumentParser* parser = document_->Parser();
  DCHECK(parser);
  parser->RemoveClient(this);
}

void HTMLImportLoader::DidRemoveAllPendingStylesheets() {
  if (state_ == kStateParsed)
    SetState(FinishLoading());
}

bool HTMLImportLoader::HasPendingResources() const {
  return document_ &&
         document_->GetStyleEngine().HasPendingScriptBlockingSheets();
}

void HTMLImportLoader::DidFinishLoading() {
  for (const auto& import_child : imports_)
    import_child->DidFinishLoading();

  ClearResource();

  DCHECK(!document_ || !document_->Parsing());
}

void HTMLImportLoader::MoveToFirst(HTMLImportChild* import) {
  wtf_size_t position = imports_.Find(import);
  DCHECK_NE(kNotFound, position);
  imports_.EraseAt(position);
  imports_.insert(0, import);
}

void HTMLImportLoader::AddImport(HTMLImportChild* import) {
  DCHECK_EQ(kNotFound, imports_.Find(import));

  imports_.push_back(import);
  import->Normalize();
  if (IsDone())
    import->DidFinishLoading();
}

void HTMLImportLoader::RemoveImport(HTMLImportChild* client) {
  DCHECK_NE(kNotFound, imports_.Find(client));
  imports_.EraseAt(imports_.Find(client));
}

bool HTMLImportLoader::ShouldBlockScriptExecution() const {
  return FirstImport()->GetState().ShouldBlockScriptExecution();
}

V0CustomElementSyncMicrotaskQueue* HTMLImportLoader::MicrotaskQueue() const {
  return microtask_queue_;
}

void HTMLImportLoader::Trace(Visitor* visitor) {
  visitor->Trace(controller_);
  visitor->Trace(imports_);
  visitor->Trace(document_);
  visitor->Trace(microtask_queue_);
  DocumentParserClient::Trace(visitor);
  RawResourceClient::Trace(visitor);
}

}  // namespace blink
