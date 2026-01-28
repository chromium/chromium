// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_

#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/smart_card.mojom-forward.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;

// Interface provided by the content embedder to support the Web Smart Card
// API.
class CONTENT_EXPORT SmartCardDelegate {
 public:
  class PermissionObserver : public base::CheckedObserver {
   public:
    // Event forwarded from
    // permissions::ObjectPermissionContextBase::PermissionObserver:
    virtual void OnPermissionRevoked(const url::Origin& origin) = 0;
  };
  // Callback type to report whether the user allowed the connection request.
  using RequestReaderPermissionCallback = base::OnceCallback<void(bool)>;

  SmartCardDelegate() = default;
  SmartCardDelegate(SmartCardDelegate&) = delete;
  SmartCardDelegate& operator=(SmartCardDelegate&) = delete;
  virtual ~SmartCardDelegate() = default;

  virtual mojo::PendingRemote<device::mojom::SmartCardContextFactory>
  GetSmartCardContextFactory(content::RenderFrameHost& render_frame_host) = 0;

  // Returns whether the origin is blocked from connecting to smart card
  // readers.
  virtual bool IsPermissionBlocked(RenderFrameHost& render_frame_host) = 0;

  // Returns whether `origin` has permission to connect to the smart card reader
  // names `reader_name`.
  //
  // Will always return false if the frame's origin IsPermissionBlocked().
  virtual bool HasReaderPermission(RenderFrameHost& render_frame_host,
                                   const std::string& reader_name) = 0;

  virtual void NotifyConnectionUsed(RenderFrameHost& render_frame_host) = 0;
  virtual void NotifyLastConnectionLost(RenderFrameHost& render_frame_host) = 0;

  virtual void AddObserver(RenderFrameHost& render_frame_host,
                           PermissionObserver* observer) = 0;
  virtual void RemoveObserver(RenderFrameHost& render_frame_host,
                              PermissionObserver* observer) = 0;

  // Shows a prompt to the user requesting permission to connect to the smart
  // card reader named `reader_name`.
  //
  // If the frame's origin IsPermissionBlocked(), `callback` will immediately
  // receive false.
  virtual void RequestReaderPermission(
      RenderFrameHost& render_frame_host,
      const std::string& reader_name,
      RequestReaderPermissionCallback callback) = 0;

  // Registers a callback to retrieve a Smart Card emulation factory for the
  // specified RenderFrameHost.
  //
  // This is used by DevTools to intercept and handle PCSC calls from the
  // renderer when Smart Card emulation is enabled. The |factory_getter|
  // will be invoked whenever the renderer requests a new
  // SmartCardContextFactory.
  virtual void SetEmulationFactory(
      content::GlobalRenderFrameHostId frame_id,
      base::RepeatingCallback<
          mojo::PendingRemote<device::mojom::SmartCardContextFactory>()>
          factory_getter) = 0;

  // Removes the emulation factory override for the specified RenderFrameHost.
  // This restores the default behavior where Smart Card requests are routed
  // to the real system PCSC service.
  virtual void ClearEmulationFactory(
      content::GlobalRenderFrameHostId frame_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SMART_CARD_DELEGATE_H_
