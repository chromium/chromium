// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SCHEMA_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SCHEMA_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/schema.h"

namespace content {
namespace protocol {

class SchemaHandler : public DevToolsDomainHandler,
                      public Schema::Backend {
 public:
  SchemaHandler();

  SchemaHandler(const SchemaHandler&) = delete;
  SchemaHandler& operator=(const SchemaHandler&) = delete;

  ~SchemaHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  Response GetDomains(
      std::unique_ptr<protocol::Array<Schema::Domain>>* domains) override;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SCHEMA_HANDLER_H_
