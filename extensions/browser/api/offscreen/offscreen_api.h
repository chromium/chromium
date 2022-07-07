// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_API_H_
#define EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_API_H_

#include "base/scoped_observation.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"

namespace extensions {

class OffscreenCreateDocumentFunction : public ExtensionFunction,
                                        public ExtensionHostObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("offscreen.createDocument",
                             OFFSCREEN_CREATEDOCUMENT);

  OffscreenCreateDocumentFunction();
  OffscreenCreateDocumentFunction(const OffscreenCreateDocumentFunction&) =
      delete;
  OffscreenCreateDocumentFunction& operator=(
      const OffscreenCreateDocumentFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~OffscreenCreateDocumentFunction() override;

  // ExtensionFunction:
  void OnBrowserContextShutdown() override;

  // ExtensionHostObserver:
  void OnExtensionHostDestroyed(ExtensionHost* host) override;
  void OnExtensionHostDidStopFirstLoad(const ExtensionHost* host) override;

  // Sends a reply asynchronously to the extension.
  void SendResponseToExtension(ResponseValue response_value);

  // Observes the newly-created document to wait for it to be ready.
  base::ScopedObservation<ExtensionHost, ExtensionHostObserver> host_observer_{
      this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_API_H_
