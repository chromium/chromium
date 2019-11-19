// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_API_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace api {
namespace events {
namespace Event {
namespace AddRules {
struct Params;
}  // namespace AddRules
namespace GetRules {
struct Params;
}  // namespace GetRules
namespace RemoveRules {
struct Params;
}  // namespace RemoveRules
}  // namespace Event
}  // namespace events
}  // namespace api

class RulesFunction : public ExtensionFunction {
 public:
  RulesFunction();

 protected:
  ~RulesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // Returns whether or not params creation succeeded, the result is used to
  // validate params.
  virtual bool CreateParams() = 0;

  // Concrete implementation of the RulesFunction that is being called
  // on the thread on which the respective RulesRegistry lives.
  // Returns false in case of errors.
  virtual ResponseValue RunAsyncOnCorrectThread() = 0;

  // Records UMA metrics for the kind of declarative API call.
  virtual void RecordUMA(const std::string& event_name) const = 0;

  scoped_refptr<RulesRegistry> rules_registry_;

 private:
  void SendResponse(ResponseValue response);

  DISALLOW_COPY_AND_ASSIGN(RulesFunction);
};

class EventsEventAddRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("events.addRules", EVENTS_ADDRULES)

  EventsEventAddRulesFunction();

 protected:
  ~EventsEventAddRulesFunction() override;

  // RulesFunction:
  bool CreateParams() override;
  ResponseValue RunAsyncOnCorrectThread() override;
  void RecordUMA(const std::string& event_name) const override;

 private:
  std::unique_ptr<api::events::Event::AddRules::Params> params_;
};

class EventsEventRemoveRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("events.removeRules", EVENTS_REMOVERULES)

  EventsEventRemoveRulesFunction();

 protected:
  ~EventsEventRemoveRulesFunction() override;

  // RulesFunction:
  bool CreateParams() override;
  ResponseValue RunAsyncOnCorrectThread() override;
  void RecordUMA(const std::string& event_name) const override;

 private:
  std::unique_ptr<api::events::Event::RemoveRules::Params> params_;
};

class EventsEventGetRulesFunction : public RulesFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("events.getRules", EVENTS_GETRULES)

  EventsEventGetRulesFunction();

 protected:
  ~EventsEventGetRulesFunction() override;

  // RulesFunction:
  bool CreateParams() override;
  ResponseValue RunAsyncOnCorrectThread() override;
  void RecordUMA(const std::string& event_name) const override;

 private:
  std::unique_ptr<api::events::Event::GetRules::Params> params_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_DECLARATIVE_API_H_
