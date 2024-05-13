// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_API_H_

#include <memory>
#include <vector>

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/common/api/cec_private.h"

namespace extensions {
namespace api {

class CecPrivateDelegate;

class CecPrivateFunction : public ExtensionFunction {
 public:
  CecPrivateFunction();

  CecPrivateFunction(const CecPrivateFunction&) = delete;
  CecPrivateFunction& operator=(const CecPrivateFunction&) = delete;

 protected:
  ~CecPrivateFunction() override;
  bool PreRunValidation(std::string* error) override;
  std::unique_ptr<CecPrivateDelegate> delegate_;

  // Callback to forward CecPrivateDelegate void(void) callback resolutions
  // up to the api caller.
  void RespondWithNoArguments(void);
};

class CecPrivateSendStandByFunction : public CecPrivateFunction {
 public:
  CecPrivateSendStandByFunction();

  CecPrivateSendStandByFunction(const CecPrivateSendStandByFunction&) = delete;
  CecPrivateSendStandByFunction& operator=(
      const CecPrivateSendStandByFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("cecPrivate.sendStandBy", CECPRIVATE_SENDSTANDBY)

 protected:
  ~CecPrivateSendStandByFunction() override;
  ResponseAction Run() override;
};

class CecPrivateSendWakeUpFunction : public CecPrivateFunction {
 public:
  CecPrivateSendWakeUpFunction();

  CecPrivateSendWakeUpFunction(const CecPrivateSendWakeUpFunction&) = delete;
  CecPrivateSendWakeUpFunction& operator=(const CecPrivateSendWakeUpFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION("cecPrivate.sendWakeUp", CECPRIVATE_SENDWAKEUP)

 protected:
  ~CecPrivateSendWakeUpFunction() override;
  ResponseAction Run() override;
};

class CecPrivateQueryDisplayCecPowerStateFunction : public CecPrivateFunction {
 public:
  CecPrivateQueryDisplayCecPowerStateFunction();

  CecPrivateQueryDisplayCecPowerStateFunction(
      const CecPrivateQueryDisplayCecPowerStateFunction&) = delete;
  CecPrivateQueryDisplayCecPowerStateFunction& operator=(
      const CecPrivateQueryDisplayCecPowerStateFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("cecPrivate.queryDisplayCecPowerState",
                             CECPRIVATE_QUERYDISPLAYCECPOWERSTATE)

 protected:
  ~CecPrivateQueryDisplayCecPowerStateFunction() override;
  ResponseAction Run() override;

 private:
  void HandlePowerStates(
      const std::vector<cec_private::DisplayCecPowerState>& power_states);
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_API_H_
