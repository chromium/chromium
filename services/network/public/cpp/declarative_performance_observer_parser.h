// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DECLARATIVE_PERFORMANCE_OBSERVER_PARSER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DECLARATIVE_PERFORMANCE_OBSERVER_PARSER_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/declarative_performance_observer.mojom.h"

namespace network {

// Parses `Performance-Observer` header and returns the parsed representation of
// it. The parsed representation is used to pass the Declarative Performance
// Observer policy between processes.
//
// Returns nullptr if parsing failed and the header should be ignored.
COMPONENT_EXPORT(NETWORK_CPP)
mojom::DeclarativePerformanceObserverPolicyPtr
ParseDeclarativePerformanceObserverPolicy(std::string_view header);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DECLARATIVE_PERFORMANCE_OBSERVER_PARSER_H_
