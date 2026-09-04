// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_

#import "components/keyed_service/core/keyed_service.h"

// Profile-scoped `KeyedService` responsible for managing web-layer proxy
// configuration and observing enterprise proxy route updates.
class ProxyServiceController : public KeyedService {
 public:
  ProxyServiceController();
  ~ProxyServiceController() override;
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_PROXY_MODEL_PROXY_SERVICE_CONTROLLER_H_
