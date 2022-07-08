// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_DOCUMENT_MANAGER_H_
#define EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_DOCUMENT_MANAGER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"

class BrowserContextKeyedServiceFactory;
class GURL;

namespace extensions {
class Extension;
class ExtensionHost;
class ProcessManager;
class OffscreenDocumentHost;

// The OffscreenDocumentManager is responsible for managing offscreen documents
// created by extensions through the `offscreen` API.
class OffscreenDocumentManager : public KeyedService,
                                 public ExtensionRegistryObserver {
 public:
  explicit OffscreenDocumentManager(content::BrowserContext* browser_context);

  OffscreenDocumentManager(const OffscreenDocumentManager&) = delete;
  OffscreenDocumentManager& operator=(const OffscreenDocumentManager&) = delete;

  ~OffscreenDocumentManager() override;

  // Returns the OffscreenDocumentManager for the given `browser_context`.
  // Note: This class has a separate instance in incognito.
  static OffscreenDocumentManager* Get(
      content::BrowserContext* browser_context);

  // Returns the KeyedServiceFactory for the OffscreenDocumentManager.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Creates and returns an offscreen document for the given `extension` and
  // `url`.
  OffscreenDocumentHost* CreateOffscreenDocument(const Extension& extension,
                                                 const GURL& url);

  // Returns the current offscreen document for the given `extension`, if one
  // exists.
  OffscreenDocumentHost* GetOffscreenDocumentForExtension(
      const Extension& extension);

  // Closes the current offscreen document. Note: This requires that there *is*
  // an active offscreen document.
  void CloseOffscreenDocumentForExtension(const Extension& extension);

 private:
  struct OffscreenDocumentData {
    // Appease Chromium clang plugin with out-of-line ctors/dtors.
    OffscreenDocumentData();
    OffscreenDocumentData(OffscreenDocumentData&&);
    ~OffscreenDocumentData();

    std::unique_ptr<OffscreenDocumentHost> host;

    // TODO(https://crbug.com/1339382): This will need more fields to include
    // attributes like the associated reason and justification for the
    // document.
  };

  // ExtensionRegistry:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // KeyedService:
  void Shutdown() override;

  // Closes the specified `offscreen_document`.
  void CloseOffscreenDocument(ExtensionHost* offscreen_document);

  // The collection of offscreen documents, mapped to extension ID.
  std::map<ExtensionId, OffscreenDocumentData> offscreen_documents_;

  // The associated browser context.
  raw_ptr<content::BrowserContext> browser_context_;

  // The process manager for the `browser_context_`.
  raw_ptr<ProcessManager> process_manager_;

  // Observe ExtensionRegistry for extensions being unloaded.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_OFFSCREEN_OFFSCREEN_DOCUMENT_MANAGER_H_
