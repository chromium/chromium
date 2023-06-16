// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_MANAGED_INTERFACE_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_MANAGED_INTERFACE_H_

#include <memory>

#include "content/common/content_export.h"

namespace content {

class WebUIController;

// Base class used by Mojo interface implementations whose lifetime is tied
// to a document and a WebUIController.
class CONTENT_EXPORT WebUIManagedInterfaceBase {
 public:
  virtual ~WebUIManagedInterfaceBase() = default;
};

// Stores a WebUIManagedInterfaceBase instance in the WebUI's Document.
CONTENT_EXPORT void SaveWebUIManagedInterfaceInDocument(
    WebUIController*,
    std::unique_ptr<WebUIManagedInterfaceBase>);

// Used by WebUIController to remove all the WebUIManagedInterfaces associated
// with it.
CONTENT_EXPORT void RemoveWebUIManagedInterfaces(
    WebUIController* webui_controller);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_MANAGED_INTERFACE_H_
