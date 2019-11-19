// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/serial/serial_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/serial_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

namespace content {

namespace {

blink::mojom::SerialPortInfoPtr ToBlinkType(
    const device::mojom::SerialPortInfo& port) {
  auto info = blink::mojom::SerialPortInfo::New();
  info->token = port.token;
  info->has_vendor_id = port.has_vendor_id;
  if (port.has_vendor_id)
    info->vendor_id = port.vendor_id;
  info->has_product_id = port.has_product_id;
  if (port.has_product_id)
    info->product_id = port.product_id;
  return info;
}

}  // namespace

SerialService::SerialService(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
  DCHECK(render_frame_host_->IsFeatureEnabled(
      blink::mojom::FeaturePolicyFeature::kSerial));
  // Serial API is not supported for back-forward cache for now because we
  // don't have support for closing/freezing ports when the frame is added to
  // the back-forward cache, so we mark frames that use this API as disabled
  // for back-forward cache.
  BackForwardCache::DisableForRenderFrameHost(render_frame_host, "Serial");

  watchers_.set_disconnect_handler(base::BindRepeating(
      &SerialService::OnWatcherConnectionError, base::Unretained(this)));
}

SerialService::~SerialService() {
  // The remaining watchers will be closed from this end.
  if (!watchers_.empty())
    DecrementActiveFrameCount();
}

void SerialService::Bind(
    mojo::PendingReceiver<blink::mojom::SerialService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SerialService::GetPorts(GetPortsCallback callback) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(std::vector<blink::mojom::SerialPortInfoPtr>());
    return;
  }

  delegate->GetPortManager(render_frame_host_)
      ->GetDevices(base::BindOnce(&SerialService::FinishGetPorts,
                                  weak_factory_.GetWeakPtr(),
                                  std::move(callback)));
}

void SerialService::RequestPort(
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    RequestPortCallback callback) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (!delegate->CanRequestPortPermission(render_frame_host_)) {
    std::move(callback).Run(nullptr);
    return;
  }

  chooser_ = delegate->RunChooser(
      render_frame_host_, std::move(filters),
      base::BindOnce(&SerialService::FinishRequestPort,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SerialService::GetPort(
    const base::UnguessableToken& token,
    mojo::PendingReceiver<device::mojom::SerialPort> receiver) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate)
    return;

  if (watchers_.empty()) {
    auto* web_contents_impl = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host_));
    web_contents_impl->IncrementSerialActiveFrameCount();
  }

  mojo::PendingRemote<device::mojom::SerialPortConnectionWatcher> watcher;
  watchers_.Add(this, watcher.InitWithNewPipeAndPassReceiver());
  delegate->GetPortManager(render_frame_host_)
      ->GetPort(token, std::move(receiver), std::move(watcher));
}

void SerialService::FinishGetPorts(
    GetPortsCallback callback,
    std::vector<device::mojom::SerialPortInfoPtr> ports) {
  std::vector<blink::mojom::SerialPortInfoPtr> result;
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate) {
    std::move(callback).Run(std::move(result));
    return;
  }

  for (const auto& port : ports) {
    if (delegate->HasPortPermission(render_frame_host_, *port))
      result.push_back(ToBlinkType(*port));
  }

  std::move(callback).Run(std::move(result));
}

void SerialService::FinishRequestPort(RequestPortCallback callback,
                                      device::mojom::SerialPortInfoPtr port) {
  SerialDelegate* delegate = GetContentClient()->browser()->GetSerialDelegate();
  if (!delegate || !port) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(ToBlinkType(*port));
}

void SerialService::OnWatcherConnectionError() {
  if (watchers_.empty())
    DecrementActiveFrameCount();
}

void SerialService::DecrementActiveFrameCount() {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(render_frame_host_));
  web_contents_impl->DecrementSerialActiveFrameCount();
}

}  // namespace content
