// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_SERVICE_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_SERVICE_H_

#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace webapps {

// Launches a new instance of the WebAppOriginAssociationParser service in an
// isolated, sandboxed process, and returns a remote interface to control the
// service. The lifetime of the process is tied to that of the Remote. May be
// called from any thread.
mojo::Remote<webapps::mojom::WebAppOriginAssociationParser>
LaunchWebAppOriginAssociationParser();

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_SERVICE_H_
