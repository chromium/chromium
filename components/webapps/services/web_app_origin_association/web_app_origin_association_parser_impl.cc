// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser_impl.h"

#include <utility>

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

namespace webapps {

WebAppOriginAssociationParserImpl::WebAppOriginAssociationParserImpl(
    mojo::PendingReceiver<webapps::mojom::WebAppOriginAssociationParser>
        receiver)
    : receiver_(this, std::move(receiver)) {}

WebAppOriginAssociationParserImpl::~WebAppOriginAssociationParserImpl() =
    default;

void WebAppOriginAssociationParserImpl::ParseWebAppOriginAssociation(
    const std::string& raw_json,
    ParseWebAppOriginAssociationCallback callback) {
  webapps::WebAppOriginAssociationParser parser;
  mojom::WebAppOriginAssociationPtr association = parser.Parse(raw_json);
  auto errors = parser.GetErrors();
  std::move(callback).Run(std::move(association), std::move(errors));
}

}  // namespace webapps
