// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printer_status.h"

#include <algorithm>
#include <iterator>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace printing {

PrinterStatus::PrinterStatus() = default;

PrinterStatus::PrinterStatus(const PrinterStatus& other) = default;

PrinterStatus::~PrinterStatus() = default;

std::string_view PrinterStatus::PrinterReason::ReasonName() const {
  switch (reason) {
    case Reason::kUnknownReason:
      return "unknown";
    case Reason::kNone:
      return "none";
    case Reason::kMediaNeeded:
      return "media-needed";
    case Reason::kMediaJam:
      return "media-jam";
    case Reason::kMovingToPaused:
      return "moving-to-paused";
    case Reason::kPaused:
      return "paused";
    case Reason::kShutdown:
      return "shutdown";
    case Reason::kConnectingToDevice:
      return "connecting-to-device";
    case Reason::kTimedOut:
      return "timed-out";
    case Reason::kStopping:
      return "stopping";
    case Reason::kStoppedPartly:
      return "stopped-partly";
    case Reason::kTonerLow:
      return "toner-low";
    case Reason::kTonerEmpty:
      return "toner-empty";
    case Reason::kSpoolAreaFull:
      return "spool-area-full";
    case Reason::kCoverOpen:
      return "cover-open";
    case Reason::kInterlockOpen:
      return "interlock-open";
    case Reason::kDoorOpen:
      return "door-open";
    case Reason::kInputTrayMissing:
      return "input-tray-missing";
    case Reason::kMediaLow:
      return "media-low";
    case Reason::kMediaEmpty:
      return "media-empty";
    case Reason::kOutputTrayMissing:
      return "output-tray-missing";
    case Reason::kOutputAreaAlmostFull:
      return "output-area-almost-full";
    case Reason::kOutputAreaFull:
      return "output-area-full";
    case Reason::kMarkerSupplyLow:
      return "marker-supply-low";
    case Reason::kMarkerSupplyEmpty:
      return "marker-supply-empty";
    case Reason::kMarkerWasteAlmostFull:
      return "marker-waste-almost-full";
    case Reason::kMarkerWasteFull:
      return "marker-waste-full";
    case Reason::kFuserOverTemp:
      return "fuser-over-temp";
    case Reason::kFuserUnderTemp:
      return "fuser-under-temp";
    case Reason::kOpcNearEol:
      return "opc-near-eol";
    case Reason::kOpcLifeOver:
      return "opc-life-over";
    case Reason::kDeveloperLow:
      return "developer-low";
    case Reason::kDeveloperEmpty:
      return "developer-empty";
    case Reason::kInterpreterResourceUnavailable:
      return "interpreter-resource-unavailable";
    case Reason::kCupsPkiExpired:
      return "cups-pki-expired";
  }
}

std::string_view PrinterStatus::PrinterReason::SeverityName() const {
  switch (severity) {
    case Severity::kUnknownSeverity:
      return "unknown";
    case Severity::kReport:
      return "report";
    case Severity::kWarning:
      return "warning";
    case Severity::kError:
      return "error";
  }
}

std::string PrinterStatus::AllReasonsAsString() const {
  std::vector<std::string> reason_strings;
  std::transform(reasons.begin(), reasons.end(),
                 std::back_inserter(reason_strings),
                 [](const PrinterStatus::PrinterReason& reason) {
                   return base::StringPrintf("%s/%s", reason.ReasonName(),
                                             reason.SeverityName());
                 });
  return base::JoinString(reason_strings, ";");
}

}  // namespace printing
