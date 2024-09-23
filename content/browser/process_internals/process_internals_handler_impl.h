// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/process_internals/process_internals.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

// Implementation of the ProcessInternalsHandler interface, which is used to
// communicate between the chrome://process-internals/ WebUI and the browser
// process.
class ProcessInternalsHandlerImpl : public ::mojom::ProcessInternalsHandler {
 public:
  ProcessInternalsHandlerImpl(
      BrowserContext* browser_context,
      mojo::PendingReceiver<::mojom::ProcessInternalsHandler> receiver);

  ProcessInternalsHandlerImpl(const ProcessInternalsHandlerImpl&) = delete;
  ProcessInternalsHandlerImpl& operator=(const ProcessInternalsHandlerImpl&) =
      delete;

  ~ProcessInternalsHandlerImpl() override;

  // mojom::ProcessInternalsHandler overrides:
  void GetProcessCountInfo(GetProcessCountInfoCallback callback) override;
  void GetIsolationMode(GetIsolationModeCallback callback) override;
  void GetProcessPerSiteMode(GetProcessPerSiteModeCallback callback) override;
  void GetUserTriggeredIsolatedOrigins(
      GetUserTriggeredIsolatedOriginsCallback callback) override;
  void GetWebTriggeredIsolatedOrigins(
      GetWebTriggeredIsolatedOriginsCallback callback) override;
  void GetGloballyIsolatedOrigins(
      GetGloballyIsolatedOriginsCallback callback) override;
  void GetAllWebContentsInfo(GetAllWebContentsInfoCallback callback) override;

 private:
  raw_ptr<BrowserContext> browser_context_;
  mojo::Receiver<::mojom::ProcessInternalsHandler> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_
