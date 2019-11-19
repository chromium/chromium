// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/request_destination.h"

namespace blink {

const char* GetRequestDestinationFromContext(
    mojom::RequestContextType context) {
  switch (context) {
    case mojom::RequestContextType::UNSPECIFIED:
    case mojom::RequestContextType::BEACON:
    case mojom::RequestContextType::DOWNLOAD:
    case mojom::RequestContextType::EVENT_SOURCE:
    case mojom::RequestContextType::FETCH:
    case mojom::RequestContextType::PING:
    case mojom::RequestContextType::XML_HTTP_REQUEST:
    case mojom::RequestContextType::SUBRESOURCE:
    case mojom::RequestContextType::PREFETCH:
      return "";
    case mojom::RequestContextType::CSP_REPORT:
      return "report";
    case mojom::RequestContextType::AUDIO:
      return "audio";
    case mojom::RequestContextType::EMBED:
      return "embed";
    case mojom::RequestContextType::FONT:
      return "font";
    case mojom::RequestContextType::FRAME:
      return "frame";
    case mojom::RequestContextType::IFRAME:
      return "iframe";
    case mojom::RequestContextType::HYPERLINK:
    case mojom::RequestContextType::LOCATION:
    case mojom::RequestContextType::FORM:
      return "document";
    case mojom::RequestContextType::IMAGE:
    case mojom::RequestContextType::FAVICON:
    case mojom::RequestContextType::IMAGE_SET:
      return "image";
    case mojom::RequestContextType::MANIFEST:
      return "manifest";
    case mojom::RequestContextType::OBJECT:
      return "object";
    case mojom::RequestContextType::SCRIPT:
      return "script";
    case mojom::RequestContextType::SERVICE_WORKER:
      return "serviceworker";
    case mojom::RequestContextType::SHARED_WORKER:
      return "sharedworker";
    case mojom::RequestContextType::STYLE:
      return "style";
    case mojom::RequestContextType::TRACK:
      return "track";
    case mojom::RequestContextType::VIDEO:
      return "video";
    case mojom::RequestContextType::WORKER:
      return "worker";
    case mojom::RequestContextType::XSLT:
      return "xslt";
    case mojom::RequestContextType::IMPORT:
    case mojom::RequestContextType::INTERNAL:
      return "unknown";

    // TODO(mkwst): We don't currently distinguish between plugin content loaded
    // via `<embed>` or `<object>` as https://github.com/whatwg/fetch/pull/948
    // asks us to do. See `content::PepperURLLoaderHost::InternalOnHostMsgOpen`
    // for details.
    case mojom::RequestContextType::PLUGIN:
      return "embed";
  }
  NOTREACHED();
  return "";
}

}  // namespace blink
