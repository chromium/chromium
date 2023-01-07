// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WEB_UI_INJECTION_HOST_H_
#define EXTENSIONS_RENDERER_WEB_UI_INJECTION_HOST_H_

#include "extensions/common/mojom/host_id.mojom-forward.h"
#include "extensions/renderer/injection_host.h"

class WebUIInjectionHost : public InjectionHost {
 public:
  WebUIInjectionHost(const extensions::mojom::HostID& host_id);

  WebUIInjectionHost(const WebUIInjectionHost&) = delete;
  WebUIInjectionHost& operator=(const WebUIInjectionHost&) = delete;

  ~WebUIInjectionHost() override;

 private:
  // InjectionHost:
  const std::string* GetContentSecurityPolicy() const override;
  const GURL& url() const override;
  const std::string& name() const override;
  extensions::PermissionsData::PageAccess CanExecuteOnFrame(
      const GURL& document_url,
      content::RenderFrame* render_frame,
      int tab_id,
      bool is_declarative) const override;

 private:
  GURL url_;
};

#endif  // EXTENSIONS_RENDERER_WEB_UI_INJECTION_HOST_H_
