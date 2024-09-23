// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_WEB_CONTENTS_PRESSURE_MANAGER_PROXY_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_WEB_CONTENTS_PRESSURE_MANAGER_PROXY_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace content {

class ScopedVirtualPressureSourceForDevTools;

class CONTENT_EXPORT WebContentsPressureManagerProxy final
    : public WebContentsUserData<WebContentsPressureManagerProxy> {
 public:
  ~WebContentsPressureManagerProxy() override;

  static WebContentsPressureManagerProxy* GetOrCreate(WebContents*);

  std::optional<base::UnguessableToken> GetTokenFor(
      device::mojom::PressureSource) const;

  std::unique_ptr<ScopedVirtualPressureSourceForDevTools>
      CreateVirtualPressureSourceForDevTools(
          device::mojom::PressureSource,
          device::mojom::VirtualPressureSourceMetadataPtr);

 private:
  friend class ScopedVirtualPressureSourceForDevTools;

  explicit WebContentsPressureManagerProxy(WebContents*);

  void EnsureDeviceServiceConnection();

  device::mojom::PressureManager* GetPressureManager();

  void OnScopedVirtualPressureSourceDevToolsDeletion(
      const ScopedVirtualPressureSourceForDevTools&);

  mojo::Remote<device::mojom::PressureManager> pressure_manager_;

  base::flat_map<device::mojom::PressureSource, base::UnguessableToken>
      virtual_pressure_sources_tokens_;

  base::WeakPtrFactory<WebContentsPressureManagerProxy> weak_ptr_factory_{this};

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// This class is the public interface to the virtual pressure source related
// Mojo calls. Instances are created by
// WebContentsPressureManagerProxy::CreateVirtualPressurSourceForDevTools().
//
// RemoveVirtualPressureSource() is invoked automatically on destruction, and
// CreateVirtualPressureSource() is invoked automatically on creation.
class CONTENT_EXPORT ScopedVirtualPressureSourceForDevTools final {
 public:
  ~ScopedVirtualPressureSourceForDevTools();

  device::mojom::PressureSource source() const;

  base::UnguessableToken token() const;

  void UpdateVirtualPressureSourceState(
      device::mojom::PressureState,
      device::mojom::PressureManager::UpdateVirtualPressureSourceStateCallback
          callback);

 private:
  friend class WebContentsPressureManagerProxy;

  ScopedVirtualPressureSourceForDevTools(
      device::mojom::PressureSource,
      device::mojom::VirtualPressureSourceMetadataPtr,
      base::WeakPtr<WebContentsPressureManagerProxy>);

  const device::mojom::PressureSource source_;

  const base::UnguessableToken token_;

  base::WeakPtr<WebContentsPressureManagerProxy>
      web_contents_pressure_manager_proxy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_WEB_CONTENTS_PRESSURE_MANAGER_PROXY_H_
