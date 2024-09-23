// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MEDIA_CAPTURE_UTIL_H_
#define EXTENSIONS_BROWSER_MEDIA_CAPTURE_UTIL_H_

#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

namespace media_capture_util {

// Grants access to audio and video capture devices.
// * If the caller requests specific device ids, grants access to those.
// * If the caller does not request specific ids, grants access to the first
//   available device.
// Usually used as a helper for media capture ProcessMediaAccessRequest().
void GrantMediaStreamRequest(content::WebContents* web_contents,
                             const content::MediaStreamRequest& request,
                             content::MediaResponseCallback callback,
                             const Extension* extension);

// Verifies that the extension has permission for |type|. If not, crash.
void VerifyMediaAccessPermission(blink::mojom::MediaStreamType type,
                                 const Extension* extension);

}  // namespace media_capture_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MEDIA_CAPTURE_UTIL_H_
