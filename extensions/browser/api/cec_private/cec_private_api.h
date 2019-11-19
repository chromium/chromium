// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_API_H_

#include <vector>

#include "chromeos/dbus/cec_service_client.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {
namespace api {

class CecPrivateFunction : public ExtensionFunction {
 public:
  CecPrivateFunction();

 protected:
  ~CecPrivateFunction() override;
  bool PreRunValidation(std::string* error) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CecPrivateFunction);
};

class CecPrivateSendStandByFunction : public CecPrivateFunction {
 public:
  CecPrivateSendStandByFunction();
  DECLARE_EXTENSION_FUNCTION("cecPrivate.sendStandBy", CECPRIVATE_SENDSTANDBY)

 protected:
  ~CecPrivateSendStandByFunction() override;
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CecPrivateSendStandByFunction);
};

class CecPrivateSendWakeUpFunction : public CecPrivateFunction {
 public:
  CecPrivateSendWakeUpFunction();
  DECLARE_EXTENSION_FUNCTION("cecPrivate.sendWakeUp", CECPRIVATE_SENDWAKEUP)

 protected:
  ~CecPrivateSendWakeUpFunction() override;
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CecPrivateSendWakeUpFunction);
};

class CecPrivateQueryDisplayCecPowerStateFunction : public CecPrivateFunction {
 public:
  CecPrivateQueryDisplayCecPowerStateFunction();
  DECLARE_EXTENSION_FUNCTION("cecPrivate.queryDisplayCecPowerState",
                             CECPRIVATE_QUERYDISPLAYCECPOWERSTATE)

 protected:
  ~CecPrivateQueryDisplayCecPowerStateFunction() override;
  ResponseAction Run() override;

 private:
  void HandlePowerStates(
      const std::vector<chromeos::CecServiceClient::PowerState>& power_states);

  DISALLOW_COPY_AND_ASSIGN(CecPrivateQueryDisplayCecPowerStateFunction);
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_API_H_
