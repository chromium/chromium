// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_API_RESOURCE_H_
#define EXTENSIONS_BROWSER_API_API_RESOURCE_H_

#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// An ApiResource represents something that an extension API manages, such as a
// socket or a serial-port connection. Typically, an ApiResourceManager will
// control the lifetime of all ApiResources of a specific derived type.
class ApiResource {
 public:
  ApiResource(const ApiResource&) = delete;
  ApiResource& operator=(const ApiResource&) = delete;

  virtual ~ApiResource();

  const ExtensionId& owner_extension_id() const { return owner_extension_id_; }

  // If this method returns |true|, the resource remains open when the
  // owning extension is suspended due to inactivity.
  virtual bool IsPersistent() const;

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::IO;

 protected:
  explicit ApiResource(const std::string& owner_extension_id);

 private:
  // The extension that owns this resource.
  const ExtensionId owner_extension_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_API_RESOURCE_H_
