// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_H_
#define EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/extension_function.h"

namespace base {
class Clock;
}  // namespace base

namespace extensions {
struct Alarm;
using AlarmList = std::vector<Alarm>;

class AlarmsCreateFunction : public ExtensionFunction {
 public:
  AlarmsCreateFunction();
  // Use |clock| instead of the default clock. Does not take ownership
  // of |clock|. Used for testing.
  explicit AlarmsCreateFunction(base::Clock* clock);

 protected:
  ~AlarmsCreateFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("alarms.create", ALARMS_CREATE)
 private:
  void Callback();

  const raw_ptr<base::Clock> clock_;
};

class AlarmsGetFunction : public ExtensionFunction {
 protected:
  ~AlarmsGetFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Callback(const std::string& name, Alarm* alarm);
  DECLARE_EXTENSION_FUNCTION("alarms.get", ALARMS_GET)
};

class AlarmsGetAllFunction : public ExtensionFunction {
 protected:
  ~AlarmsGetAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Callback(const AlarmList* alarms);
  DECLARE_EXTENSION_FUNCTION("alarms.getAll", ALARMS_GETALL)
};

class AlarmsClearFunction : public ExtensionFunction {
 protected:
  ~AlarmsClearFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Callback(const std::string& name, bool success);
  DECLARE_EXTENSION_FUNCTION("alarms.clear", ALARMS_CLEAR)
};

class AlarmsClearAllFunction : public ExtensionFunction {
 protected:
  ~AlarmsClearAllFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void Callback();
  DECLARE_EXTENSION_FUNCTION("alarms.clearAll", ALARMS_CLEARALL)
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ALARMS_ALARMS_API_H_
