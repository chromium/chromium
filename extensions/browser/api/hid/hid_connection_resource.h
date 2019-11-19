// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_HID_HID_CONNECTION_RESOURCE_H_
#define EXTENSIONS_BROWSER_API_HID_HID_CONNECTION_RESOURCE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace extensions {

class HidConnectionResource : public ApiResource {
 public:
  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

  HidConnectionResource(
      const std::string& owner_extension_id,
      mojo::PendingRemote<device::mojom::HidConnection> connection);
  ~HidConnectionResource() override;

  device::mojom::HidConnection* connection() const { return connection_.get(); }

  bool IsPersistent() const override;

  static const char* service_name() { return "HidConnectionResourceManager"; }

 private:
  mojo::Remote<device::mojom::HidConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionResource);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_HID_HID_CONNECTION_RESOURCE_H_
