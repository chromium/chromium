// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/diagnostics/diagnostics_api.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"

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
  std::optional<base::Value> parsed_value(base::JSONReader::Read(status));
  if (!parsed_value || !parsed_value->is_dict())
    return false;

  base::Value::Dict& result = parsed_value->GetDict();
  if (result.size() != 1)
    return false;

  // Returns the first item.
  base::Value::Dict::iterator iterator = result.begin();
  if (!iterator->second.is_dict())
    return false;

  std::optional<double> avg = iterator->second.GetDict().FindDouble("avg");
  if (!avg)
    return false;
  *latency = *avg;

  *ip = iterator->first;
  return true;
}

}  // namespace

namespace SendPacket = api::diagnostics::SendPacket;

DiagnosticsSendPacketFunction::DiagnosticsSendPacketFunction() = default;
DiagnosticsSendPacketFunction::~DiagnosticsSendPacketFunction() = default;

ExtensionFunction::ResponseAction DiagnosticsSendPacketFunction::Run() {
  auto params = api::diagnostics::SendPacket::Params::Create(args());

  std::map<std::string, std::string> config;
  config[kCount] = kDefaultCount;
  if (params->options.ttl)
    config[kTTL] = base::NumberToString(*params->options.ttl);
  if (params->options.timeout)
    config[kTimeout] = base::NumberToString(*params->options.timeout);
  if (params->options.size)
    config[kSize] = base::NumberToString(*params->options.size);

  ash::DebugDaemonClient::Get()->TestICMPWithOptions(
      params->options.ip, config,
      base::BindOnce(&DiagnosticsSendPacketFunction::OnTestICMPCompleted,
                     this));

  return RespondLater();
}

void DiagnosticsSendPacketFunction::OnTestICMPCompleted(
    std::optional<std::string> status) {
  std::string ip;
  double latency;
  if (!status.has_value() || !ParseResult(status.value(), &ip, &latency)) {
    Respond(Error(kErrorPingFailed));
    return;
  }

  api::diagnostics::SendPacketResult result;
  result.ip = ip;
  result.latency = latency;
  Respond(WithArguments(SendPacket::Results::Create(result)));
}

}  // namespace extensions
