// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTER_STATUS_H_
#define PRINTING_PRINTER_STATUS_H_

#include <cups/cups.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"

namespace printing {

// Represents the status of a printer containing the properties printer-state,
// printer-state-reasons, and printer-state-message.
struct COMPONENT_EXPORT(PRINTING_BASE) PrinterStatus {
  struct PrinterReason {
    // This enum is used to record UMA histogram values and should not be
    // reordered. Please keep in sync with PrinterStatusReasons in
    // src/tools/metrics/histograms/enums.xml.
    enum class Reason {
      kUnknownReason = 0,
      kNone = 1,
      kMediaNeeded = 2,
      kMediaJam = 3,
      kMovingToPaused = 4,
      kPaused = 5,
      kShutdown = 6,
      kConnectingToDevice = 7,
      kTimedOut = 8,
      kStopping = 9,
      kStoppedPartly = 10,
      kTonerLow = 11,
      kTonerEmpty = 12,
      kSpoolAreaFull = 13,
      kCoverOpen = 14,
      kInterlockOpen = 15,
      kDoorOpen = 16,
      kInputTrayMissing = 17,
      kMediaLow = 18,
      kMediaEmpty = 19,
      kOutputTrayMissing = 20,
      kOutputAreaAlmostFull = 21,
      kOutputAreaFull = 22,
      kMarkerSupplyLow = 23,
      kMarkerSupplyEmpty = 24,
      kMarkerWasteAlmostFull = 25,
      kMarkerWasteFull = 26,
      kFuserOverTemp = 27,
      kFuserUnderTemp = 28,
      kOpcNearEol = 29,
      kOpcLifeOver = 30,
      kDeveloperLow = 31,
      kDeveloperEmpty = 32,
      kInterpreterResourceUnavailable = 33,
      kCupsPkiExpired = 34,
      kMaxValue = kCupsPkiExpired
    };

    // Severity of the state-reason.
    enum class Severity {
      kUnknownSeverity = 0,
      kReport = 1,
      kWarning = 2,
      kError = 3,
    };

    Reason reason;
    Severity severity;

    std::string_view ReasonName() const;
    std::string_view SeverityName() const;
  };

  PrinterStatus();
  PrinterStatus(const PrinterStatus& other);
  ~PrinterStatus();

  // printer-state
  ipp_pstate_t state;
  // printer-state-reasons
  std::vector<PrinterReason> reasons;
  // printer-state-message
  std::string message;

  std::string AllReasonsAsString() const;
};

}  // namespace printing

#endif  // PRINTING_PRINTER_STATUS_H_
