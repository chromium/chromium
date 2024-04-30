// Copyright 2022 The Chromium Authors
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
                             OFFSCREEN_CREATEDOCUMENT)

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

  // Called (asynchronously) if the page fails to load.
  void NotifyPageFailedToLoad();

  // Sends a reply asynchronously to the extension.
  void SendResponseToExtension(ResponseValue response_value);

  // Observes the newly-created document to wait for it to be ready.
  base::ScopedObservation<ExtensionHost, ExtensionHostObserver> host_observer_{
      this};
};

class OffscreenCloseDocumentFunction : public ExtensionFunction,
                                       public ExtensionHostObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("offscreen.closeDocument", OFFSCREEN_CLOSEDOCUMENT)

  OffscreenCloseDocumentFunction();
  OffscreenCloseDocumentFunction(const OffscreenCloseDocumentFunction&) =
      delete;
  OffscreenCloseDocumentFunction& operator=(
      const OffscreenCloseDocumentFunction&) = delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~OffscreenCloseDocumentFunction() override;

  // ExtensionFunction:
  void OnBrowserContextShutdown() override;

  // ExtensionHostObserver:
  void OnExtensionHostDestroyed(ExtensionHost* host) override;

  // Sends a reply asynchronously to the extension.
  void SendResponseToExtension(ResponseValue response_value);

  // Observes the newly-created document to wait for it to be ready.
  base::ScopedObservation<ExtensionHost, ExtensionHostObserver> host_observer_{
      this};
};

class OffscreenHasDocumentFunction : public ExtensionFunction,
                                     public ExtensionHostObserver {
 public:
  // Note: We use `UNKNOWN` as the histogram value here because we are unlikely
  // to ship this API to stable.
  DECLARE_EXTENSION_FUNCTION("offscreen.hasDocument", UNKNOWN)

  OffscreenHasDocumentFunction();
  OffscreenHasDocumentFunction(const OffscreenHasDocumentFunction&) = delete;
  OffscreenHasDocumentFunction& operator=(const OffscreenHasDocumentFunction&) =
      delete;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  ~OffscreenHasDocumentFunction() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_API_H_
