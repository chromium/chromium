// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/hid_chooser.h"
#include "services/device/public/mojom/hid.mojom-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class RenderFrameHost;
class WebContents;

class CONTENT_EXPORT HidDelegate {
 public:
  virtual ~HidDelegate() = default;

  // Shows a chooser for the user to select a HID device. |callback| will be
  // run when the prompt is closed. Deleting the returned object will cancel the
  // prompt.
  virtual std::unique_ptr<HidChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      HidChooser::Callback callback) = 0;

  // Returns whether the main frame owned by |web_contents| has permission to
  // request access to a device. |requesting_origin| is the origin of the
  // frame that would make the request.
  virtual bool CanRequestDevicePermission(
      WebContents* web_contents,
      const url::Origin& requesting_origin) = 0;

  // Returns whether the main frame owned by |web_contents| has permission to
  // access |device|. |requesting_origin| is the origin of the frame making
  // the request.
  virtual bool HasDevicePermission(
      WebContents* web_contents,
      const url::Origin& requesting_origin,
      const device::mojom::HidDeviceInfo& device) = 0;

  // Returns an open connection to the HidManager interface owned by the
  // embedder and being used to serve requests from |web_contents|.
  //
  // Content and the embedder must use the same connection so that the embedder
  // can process connect/disconnect events for permissions management purposes
  // before they are delivered to content. Otherwise race conditions are
  // possible.
  virtual device::mojom::HidManager* GetHidManager(
      WebContents* web_contents) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HID_DELEGATE_H_
