// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SERVICE_FACTORY_H_
#define CONTENT_BROWSER_MEDIA_SERVICE_FACTORY_H_

#include "base/token.h"
#include "build/build_config.h"
#include "content/public/common/cdm_info.h"
#include "media/mojo/mojom/cdm_service.mojom-forward.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "media/mojo/mojom/media_foundation_service.mojom-forward.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

class BrowserContext;

// Gets an instance of the CdmService for the `browser_context`, `site`, and
// `cdm_info`. Instances are started lazily as needed. The CDM located at
// `cdm_info` is loaded in the sandboxed process to be used by the service.
media::mojom::CdmService& GetCdmService(BrowserContext* browser_context,
                                        const GURL& site,
                                        const CdmInfo& cdm_info);

#if BUILDFLAG(IS_WIN)
// Gets an instance of the MediaFoundationService for the `browser_context` and
// the `site`. Instances are started lazily as needed. The CDM located at
// `cdm_path` is loaded in the sandboxed process to be used by the service.
media::mojom::MediaFoundationService& GetMediaFoundationService(
    BrowserContext* browser_context,
    const GURL& site,
    const base::FilePath& cdm_path);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SERVICE_FACTORY_H_
