// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_service.h"

#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace webapps {

mojo::Remote<webapps::mojom::WebAppOriginAssociationParser>
LaunchWebAppOriginAssociationParser() {
  return content::ServiceProcessHost::Launch<
      webapps::mojom::WebAppOriginAssociationParser>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("Web App Origin Association Parser Service")
          .Pass());
}

}  // namespace webapps
