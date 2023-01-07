// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_INJECTION_HOST_H_
#define EXTENSIONS_RENDERER_INJECTION_HOST_H_

#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

namespace content {
class RenderFrame;
}

// An interface for all kinds of hosts who own user scripts.
class InjectionHost {
 public:
  InjectionHost(const extensions::mojom::HostID& host_id);

  InjectionHost(const InjectionHost&) = delete;
  InjectionHost& operator=(const InjectionHost&) = delete;

  virtual ~InjectionHost();

  // Returns the CSP to be used for the isolated world. Currently this only
  // bypasses the main world CSP. If null is returned, the main world CSP is not
  // bypassed.
  virtual const std::string* GetContentSecurityPolicy() const = 0;

  // The base url for the host.
  virtual const GURL& url() const = 0;

  // The human-readable name of the host.
  virtual const std::string& name() const = 0;

  // Returns true if the script should execute.
  virtual extensions::PermissionsData::PageAccess CanExecuteOnFrame(
      const GURL& document_url,
      content::RenderFrame* render_frame,
      int tab_id,
      bool is_declarative) const = 0;

  const extensions::mojom::HostID& id() const { return id_; }

 private:
  // The ID of the host.
  extensions::mojom::HostID id_;
};

#endif  // EXTENSIONS_RENDERER_INJECTION_HOST_H_
