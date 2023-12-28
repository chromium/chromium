// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MODEL_NET_TYPES_H_
#define IOS_CHROME_BROWSER_NET_MODEL_NET_TYPES_H_

#include <map>
#include <memory>
#include <string>

#include "net/url_request/url_request_job_factory.h"

// A mapping from the scheme name to the protocol handler that services its
// content.
using ProtocolHandlerMap =
    std::map<std::string,
             std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>>;

#endif  // IOS_CHROME_BROWSER_NET_MODEL_NET_TYPES_H_
