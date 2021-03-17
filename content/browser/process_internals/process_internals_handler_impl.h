// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_

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
  ~ProcessInternalsHandlerImpl() override;

  // mojom::ProcessInternalsHandler overrides:
  void GetIsolationMode(GetIsolationModeCallback callback) override;
  void GetUserTriggeredIsolatedOrigins(
      GetUserTriggeredIsolatedOriginsCallback callback) override;
  void GetGloballyIsolatedOrigins(
      GetGloballyIsolatedOriginsCallback callback) override;
  void GetAllWebContentsInfo(GetAllWebContentsInfoCallback callback) override;

 private:
  BrowserContext* browser_context_;
  mojo::Receiver<::mojom::ProcessInternalsHandler> receiver_;

  DISALLOW_COPY_AND_ASSIGN(ProcessInternalsHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROCESS_INTERNALS_PROCESS_INTERNALS_HANDLER_IMPL_H_
