// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/offscreen_api.h"

#include "extensions/browser/api/offscreen/offscreen_document_manager.h"
#include "extensions/browser/offscreen_document_host.h"
#include "extensions/common/api/offscreen.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

OffscreenCreateDocumentFunction::OffscreenCreateDocumentFunction() = default;
OffscreenCreateDocumentFunction::~OffscreenCreateDocumentFunction() = default;

ExtensionFunction::ResponseAction OffscreenCreateDocumentFunction::Run() {
  std::unique_ptr<api::offscreen::CreateDocument::Params> params(
      api::offscreen::CreateDocument::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params);
  EXTENSION_FUNCTION_VALIDATE(extension());

  // TODO(https://crbug.com/1339382): This method is not complete. We need more
  // robust URL handling, to check if the extension already has an offscreen
  // document, to properly handle incognito, etc.

  const GURL url = extension()->GetResourceURL(params->parameters.url);
  CHECK_EQ(extension()->origin(), url::Origin::Create(url));

  OffscreenDocumentManager* manager =
      OffscreenDocumentManager::Get(browser_context());
  OffscreenDocumentHost* offscreen_document =
      manager->CreateOffscreenDocument(*extension(), url);
  DCHECK(offscreen_document);

  // We assume it's impossible for a document to entirely synchronously load. If
  // that ever changes, we'll need to update this to check the status of the
  // load and respond synchronously.
  DCHECK(!offscreen_document->has_loaded_once());

  host_observer_.Observe(offscreen_document);

  // Add a reference so that we can respond to the extension once the
  // offscreen document finishes its initial load.
  // Balanced in either `OnBrowserContextShutdown()` or
  // `SendResponseToExtension()`.
  AddRef();

  return RespondLater();
}

void OffscreenCreateDocumentFunction::OnBrowserContextShutdown() {
  // Release dangling lifetime pointers and bail. No point in responding now;
  // the context is shutting down. Reset `host_observer_` first to allay any
  // re-entrancy concerns about the host being destructed at this point.
  host_observer_.Reset();
  Release();  // Balanced in Run().
}

void OffscreenCreateDocumentFunction::OnExtensionHostDestroyed(
    ExtensionHost* host) {
  SendResponseToExtension(
      Error("Offscreen document closed before fully loading."));
}

void OffscreenCreateDocumentFunction::OnExtensionHostDidStopFirstLoad(
    const ExtensionHost* host) {
  SendResponseToExtension(NoArguments());
}

void OffscreenCreateDocumentFunction::SendResponseToExtension(
    ResponseValue response_value) {
  DCHECK(browser_context())
      << "SendResponseToExtension() should never be called after context "
      << "shutdown";
  Respond(std::move(response_value));
  Release();  // Balanced in Run().
}

}  // namespace extensions
