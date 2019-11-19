// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_POWER_SOURCE_SYSTEM_POWER_SOURCE_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_POWER_SOURCE_SYSTEM_POWER_SOURCE_API_H_

#include "base/scoped_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class SystemPowerSourceAPI : public BrowserContextKeyedAPI,
                             public chromeos::PowerManagerClient::Observer {
 public:
  explicit SystemPowerSourceAPI(content::BrowserContext* context);
  ~SystemPowerSourceAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SystemPowerSourceAPI>*
  GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SystemPowerSourceAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SystemPowerSourceAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  // Overridden from PowerManagerClient::Observer.
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  content::BrowserContext* const browser_context_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(SystemPowerSourceAPI);
};

class SystemPowerSourceGetPowerSourceInfoFunction : public ExtensionFunction {
 public:
  SystemPowerSourceGetPowerSourceInfoFunction();

  DECLARE_EXTENSION_FUNCTION("system.powerSource.getPowerSourceInfo",
                             SYSTEM_POWER_SOURCE_GETPOWERSOURCEINFO)

 protected:
  ~SystemPowerSourceGetPowerSourceInfoFunction() override;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemPowerSourceGetPowerSourceInfoFunction);
};

class SystemPowerSourceRequestStatusUpdateFunction : public ExtensionFunction {
 public:
  SystemPowerSourceRequestStatusUpdateFunction();

  DECLARE_EXTENSION_FUNCTION("system.powerSource.requestStatusUpdate",
                             SYSTEM_POWER_SOURCE_REQUESTSTATUSUPDATE)

 protected:
  ~SystemPowerSourceRequestStatusUpdateFunction() override;

  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemPowerSourceRequestStatusUpdateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_POWER_SOURCE_SYSTEM_POWER_SOURCE_API_H_
