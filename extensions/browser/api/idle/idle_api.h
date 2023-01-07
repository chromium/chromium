// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_IDLE_IDLE_API_H_
#define EXTENSIONS_BROWSER_API_IDLE_IDLE_API_H_

#include "extensions/browser/extension_function.h"
#include "ui/base/idle/idle.h"

namespace extensions {

// Implementation of the chrome.idle.queryState API.
class IdleQueryStateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.queryState", IDLE_QUERYSTATE)

 protected:
  ~IdleQueryStateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void IdleStateCallback(ui::IdleState state);
};

// Implementation of the chrome.idle.setDetectionInterval API.
class IdleSetDetectionIntervalFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.setDetectionInterval",
                             IDLE_SETDETECTIONINTERVAL)

 protected:
  ~IdleSetDetectionIntervalFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class IdleGetAutoLockDelayFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.getAutoLockDelay", IDLE_GETAUTOLOCKDELAY)

 protected:
  ~IdleGetAutoLockDelayFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_IDLE_IDLE_API_H_
