// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_INJECTION_HOST_H_
#define EXTENSIONS_RENDERER_EXTENSION_INJECTION_HOST_H_

#include "base/memory/raw_ptr.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/renderer/injection_host.h"

namespace extensions {

// A wrapper class that holds an extension and implements the InjectionHost
// interface.
class ExtensionInjectionHost : public InjectionHost {
 public:
  ExtensionInjectionHost(const Extension* extension);

  ExtensionInjectionHost(const ExtensionInjectionHost&) = delete;
  ExtensionInjectionHost& operator=(const ExtensionInjectionHost&) = delete;

  ~ExtensionInjectionHost() override;

  // Create an ExtensionInjectionHost object. If the extension is gone, returns
  // a null scoped ptr.
  static std::unique_ptr<const InjectionHost> Create(
      const ExtensionId& extension_id);

 private:
  // InjectionHost:
  const std::string* GetContentSecurityPolicy() const override;
  const GURL& url() const override;
  const std::string& name() const override;
  PermissionsData::PageAccess CanExecuteOnFrame(
      const GURL& document_url,
      content::RenderFrame* render_frame,
      int tab_id,
      bool is_declarative) const override;

  raw_ptr<const Extension, DanglingUntriaged> extension_;
};

}  // namespace extesions

#endif  // EXTENSIONS_RENDERER_EXTENSION_INJECTION_HOST_H_
