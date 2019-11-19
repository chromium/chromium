// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/diagnostics/diagnostics_api.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"

namespace extensions {

namespace {

const char kErrorPingFailed[] = "Failed to send ping packet";

const char kCount[] = "count";
const char kDefaultCount[] = "1";
const char kTTL[] = "ttl";
const char kTimeout[] = "timeout";
const char kSize[] = "size";

bool ParseResult(const std::string& status, std::string* ip, double* latency) {
  // Parses the result and returns IP and latency.
  std::unique_ptr<base::Value> parsed_value(
      base::JSONReader::ReadDeprecated(status));
  if (!parsed_value)
    return false;

  base::DictionaryValue* result = NULL;
  if (!parsed_value->GetAsDictionary(&result) || result->size() != 1)
    return false;

  // Returns the first item.
  base::DictionaryValue::Iterator iterator(*result);

  const base::DictionaryValue* info;
  if (!iterator.value().GetAsDictionary(&info))
    return false;

  if (!info->GetDouble("avg", latency))
    return false;

  *ip = iterator.key();
  return true;
}

}  // namespace

namespace SendPacket = api::diagnostics::SendPacket;

DiagnosticsSendPacketFunction::DiagnosticsSendPacketFunction() = default;
DiagnosticsSendPacketFunction::~DiagnosticsSendPacketFunction() = default;

ExtensionFunction::ResponseAction DiagnosticsSendPacketFunction::Run() {
  auto params = api::diagnostics::SendPacket::Params::Create(*args_);

  std::map<std::string, std::string> config;
  config[kCount] = kDefaultCount;
  if (params->options.ttl)
    config[kTTL] = base::NumberToString(*params->options.ttl);
  if (params->options.timeout)
    config[kTimeout] = base::NumberToString(*params->options.timeout);
  if (params->options.size)
    config[kSize] = base::NumberToString(*params->options.size);

  chromeos::DBusThreadManager::Get()
      ->GetDebugDaemonClient()
      ->TestICMPWithOptions(
          params->options.ip, config,
          base::BindOnce(&DiagnosticsSendPacketFunction::OnTestICMPCompleted,
                         this));

  return RespondLater();
}

void DiagnosticsSendPacketFunction::OnTestICMPCompleted(
    base::Optional<std::string> status) {
  std::string ip;
  double latency;
  if (!status.has_value() || !ParseResult(status.value(), &ip, &latency)) {
    Respond(Error(kErrorPingFailed));
    return;
  }

  api::diagnostics::SendPacketResult result;
  result.ip = ip;
  result.latency = latency;
  Respond(OneArgument(SendPacket::Results::Create(result)));
}

}  // namespace extensions
