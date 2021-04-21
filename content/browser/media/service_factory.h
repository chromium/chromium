// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SERVICE_FACTORY_H_
#define CONTENT_BROWSER_MEDIA_SERVICE_FACTORY_H_

#include "base/token.h"
#include "build/build_config.h"
#include "content/public/common/cdm_info.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#endif  // defined(OS_WIN)

namespace content {

class BrowserContext;

// Gets an instance of the CdmService for the CDM `guid`, `browser_context` and
// the `site`. Instances are started lazily as needed.
media::mojom::CdmService& GetCdmService(const base::Token& guid,
                                        BrowserContext* browser_context,
                                        const GURL& site,
                                        const CdmInfo& cdm_info);

#if defined(OS_WIN)
// Gets an instance of the MediaFoundationService for the `browser_context` and
// the `site`. Instances are started lazily as needed.
// TODO(xhwang): Not separating MediaFoundationService by CDM `guid` because we
// run both the CDM and media Renderer in the service, and when connecting to
// media Renderer, we don't know which CDM `guid` to use.
media::mojom::MediaFoundationService& GetMediaFoundationService(
    BrowserContext* browser_context,
    const GURL& site);
#endif  // defined(OS_WIN)

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SERVICE_FACTORY_H_
