// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_AGENT_CLUSTER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_AGENT_CLUSTER_PARSER_H_

#include <string>
#include "base/component_export.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"

namespace network {

// This part of the spec specifically handles header parsing:
// https://html.spec.whatwg.org/C/#initialise-the-document-object
//
// See the comment in network::PopulateParsedHeaders for restrictions on this
// function.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::OriginAgentClusterValue ParseOriginAgentCluster(const std::string&);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_ORIGIN_AGENT_CLUSTER_PARSER_H_
