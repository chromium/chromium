// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_IMPL_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_IMPL_H_

#include <string>

#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace webapps {

// Implementation of the WebAppOriginAssociationParser mojom interface.
class WebAppOriginAssociationParserImpl
    : public mojom::WebAppOriginAssociationParser {
 public:
  // Constructs a WebAppOriginAssociationParserImpl bound to |receiver|.
  explicit WebAppOriginAssociationParserImpl(
      mojo::PendingReceiver<mojom::WebAppOriginAssociationParser> receiver);
  ~WebAppOriginAssociationParserImpl() override;
  WebAppOriginAssociationParserImpl(const WebAppOriginAssociationParserImpl&) =
      delete;
  WebAppOriginAssociationParserImpl& operator=(
      const WebAppOriginAssociationParserImpl&) = delete;

  // webapps::mojom::WebAppOriginAssociationParser:
  void ParseWebAppOriginAssociation(
      const std::string& raw_json,
      ParseWebAppOriginAssociationCallback callback) override;

 private:
  mojo::Receiver<mojom::WebAppOriginAssociationParser> receiver_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_IMPL_H_
