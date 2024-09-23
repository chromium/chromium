// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_INTERNALS_HANDLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/xr/service/xr_runtime_manager_impl.h"
#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

// Implementation of the WebXrInternalsHandler interface, which is used to
// communicate between the chrome://webxr-internals WebUI and the browser
// process.
class WebXrInternalsHandlerImpl : public webxr::mojom::WebXrInternalsHandler {
 public:
  explicit WebXrInternalsHandlerImpl(
      mojo::PendingReceiver<webxr::mojom::WebXrInternalsHandler> receiver,
      WebContents* web_contents);

  WebXrInternalsHandlerImpl(const WebXrInternalsHandlerImpl&) = delete;
  WebXrInternalsHandlerImpl& operator=(const WebXrInternalsHandlerImpl&) =
      delete;

  ~WebXrInternalsHandlerImpl() override;

  // webxr::mojom::WebXrInternalsHandler overrides:
  void GetDeviceInfo(GetDeviceInfoCallback callback) override;
  void GetActiveRuntimes(GetActiveRuntimesCallback callback) override;
  void SubscribeToEvents(
      mojo::PendingRemote<webxr::mojom::XRInternalsSessionListener>
          pending_remote) override;

 private:
  mojo::Receiver<webxr::mojom::WebXrInternalsHandler> receiver_;
  scoped_refptr<XRRuntimeManagerImpl> runtime_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_INTERNALS_HANDLER_IMPL_H_
