// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/api/bluetooth/bluetooth_api.h"
#include "extensions/browser/api/bluetooth/bluetooth_event_router.h"

using content::BrowserThread;

namespace {

const char kPlatformNotSupported[] =
    "This operation is not supported on your platform";

extensions::BluetoothEventRouter* GetEventRouter(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return extensions::BluetoothAPI::Get(context)->event_router();
}

bool IsBluetoothSupported(content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return GetEventRouter(context)->IsBluetoothSupported();
}

void GetAdapter(device::BluetoothAdapterFactory::AdapterCallback callback,
                content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetEventRouter(context)->GetAdapter(std::move(callback));
}

}  // namespace

namespace extensions {
namespace api {

BluetoothExtensionFunction::BluetoothExtensionFunction() {
}

BluetoothExtensionFunction::~BluetoothExtensionFunction() {
}

ExtensionFunction::ResponseAction BluetoothExtensionFunction::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  EXTENSION_FUNCTION_VALIDATE(CreateParams());

  if (!IsBluetoothSupported(browser_context())) {
    return RespondNow(Error(kPlatformNotSupported));
  }

  GetAdapter(
      base::BindOnce(&BluetoothExtensionFunction::RunOnAdapterReady, this),
      browser_context());
  return did_respond() ? AlreadyResponded() : RespondLater();
}

bool BluetoothExtensionFunction::CreateParams() {
  return true;
}

std::string BluetoothExtensionFunction::GetExtensionId() {
  if (extension()) {
    return extension()->id();
  }
  return render_frame_host()->GetLastCommittedURL().host();
}

void BluetoothExtensionFunction::RunOnAdapterReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DoWork(adapter);
}

}  // namespace api
}  // namespace extensions
