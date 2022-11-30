// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_DBUS_STATISTICS_H_
#define DBUS_DBUS_STATISTICS_H_

#include <string>

#include "dbus/dbus_export.h"

// The functions defined here are used to gather DBus statistics, and
// provide them in a format convenient for debugging. These functions are only
// valid when called from the main thread (the thread which Initialize() was
// called from). Calls from other threads will be ignored.

namespace dbus {
namespace statistics {

// Enum to specify what level of detail to show in GetAsString
enum ShowInString {
  SHOW_SERVICE = 0,  // Service totals only
  SHOW_INTERFACE = 1,  // Service + interface totals
  SHOW_METHOD = 2,  // Service + interface + method totals
};

// Enum to specify how to format the display in GetAsString
enum FormatString {
  FORMAT_TOTALS = 0,  // Raw totals only
  FORMAT_PER_MINUTE = 1,  // Per-minute only
  FORMAT_ALL = 2  // Include all format details
};

// Initializes / shuts down dbus statistics gathering. Calling Initialize
// more than once will reset the statistics.
CHROME_DBUS_EXPORT void Initialize();
CHROME_DBUS_EXPORT void Shutdown();

// Add sent/received calls to the statistics gathering class. These methods
// do nothing unless Initialize() was called.
CHROME_DBUS_EXPORT void AddSentMethodCall(const std::string& service,
                                          const std::string& interface,
                                          const std::string& method);
CHROME_DBUS_EXPORT void AddReceivedSignal(const std::string& service,
                                          const std::string& interface,
                                          const std::string& method);
// Track synchronous calls independently since we want to highlight
// (and remove) these.
CHROME_DBUS_EXPORT void AddBlockingSentMethodCall(const std::string& service,
                                                  const std::string& interface,
                                                  const std::string& method);

// Output the calls into a formatted string. |show| determines what level
// of detail to show: one line per service, per interface, or per method.
// If |show_per_minute| is true include per minute stats.
// Example output for SHOW_METHOD, FORMAT_TOTALS:
//   org.chromium.Mtpd.EnumerateStorage: Sent: 100
//   org.chromium.Mtpd.MTPStorageSignal: Received: 20
// Example output for SHOW_INTERFACE, FORMAT_ALL:
//   org.chromium.Mtpd: Sent: 100 (10/min) Received: 20 (2/min)
CHROME_DBUS_EXPORT std::string GetAsString(ShowInString show,
                                           FormatString format);

namespace testing {
// Sets |sent| to the number of sent calls, |received| to the number of
// received calls, and |blocking| to the number of sent blocking calls for
// service+interface+method. Used in unittests.
CHROME_DBUS_EXPORT bool GetCalls(const std::string& service,
                                 const std::string& interface,
                                 const std::string& method,
                                 int* sent,
                                 int* received,
                                 int* blocking);
}  // namespace testing

}  // namespace statistics
}  // namespace dbus

#endif  // DBUS_DBUS_STATISTICS_H_
