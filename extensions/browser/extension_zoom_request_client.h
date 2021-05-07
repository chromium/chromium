// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ZOOM_REQUEST_CLIENT_H_
#define EXTENSIONS_BROWSER_EXTENSION_ZOOM_REQUEST_CLIENT_H_

#include "base/macros.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/common/extension.h"

namespace extensions {

class Extension;

// This class implements ZoomRequestClient in order to encapsulate a ref pointer
// back to an extension requesting a zoom level change. This is important so
// that zoom event observers can determine if an extension made the request
// as opposed to direct user input.
class ExtensionZoomRequestClient : public zoom::ZoomRequestClient {
 public:
  explicit ExtensionZoomRequestClient(scoped_refptr<const Extension> extension);

  bool ShouldSuppressBubble() const override;
  const Extension* extension() const { return extension_.get(); }

 protected:
  ~ExtensionZoomRequestClient() override;

 private:
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionZoomRequestClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ZOOM_REQUEST_CLIENT_H_
