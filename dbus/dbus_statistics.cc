// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/dbus_statistics.h"

#include <map>
#include <tuple>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace dbus {

namespace {

struct StatKey {
  std::string service;
  std::string interface;
  std::string method;
};

bool operator<(const StatKey& lhs, const StatKey& rhs) {
  return std::tie(lhs.service, lhs.interface, lhs.method) <
         std::tie(rhs.service, rhs.interface, rhs.method);
}

struct StatValue {
  int sent_method_calls = 0;
  int received_signals = 0;
  int sent_blocking_method_calls = 0;
};

using StatMap = std::map<StatKey, StatValue>;

//------------------------------------------------------------------------------
// DBusStatistics

// Simple class for gathering DBus usage statistics.
class DBusStatistics {
 public:
  DBusStatistics()
      : start_time_(base::Time::Now()),
        origin_thread_id_(base::PlatformThread::CurrentId()) {
  }

  DBusStatistics(const DBusStatistics&) = delete;
  DBusStatistics& operator=(const DBusStatistics&) = delete;

  ~DBusStatistics() {
    DCHECK_EQ(origin_thread_id_, base::PlatformThread::CurrentId());
  }

  // Enum to specify which field in Stat to increment in AddStat.
  enum StatType {
    TYPE_SENT_METHOD_CALLS,
    TYPE_RECEIVED_SIGNALS,
    TYPE_SENT_BLOCKING_METHOD_CALLS
  };

  // Add a call to |method| for |interface|. See also MethodCall in message.h.
  void AddStat(const std::string& service,
               const std::string& interface,
               const std::string& method,
               StatType type) {
    if (base::PlatformThread::CurrentId() != origin_thread_id_) {
      DVLOG(1) << "Ignoring DBusStatistics::AddStat call from thread: "
               << base::PlatformThread::CurrentId();
      return;
    }
    StatValue* stat = GetStats(service, interface, method, true);
    DCHECK(stat);
    if (type == TYPE_SENT_METHOD_CALLS)
      ++stat->sent_method_calls;
    else if (type == TYPE_RECEIVED_SIGNALS)
      ++stat->received_signals;
    else if (type == TYPE_SENT_BLOCKING_METHOD_CALLS)
      ++stat->sent_blocking_method_calls;
    else
      NOTREACHED();
  }

  // Look up the Stat entry in |stats_|. If |add_stat| is true, add a new entry
  // if one does not already exist.
  StatValue* GetStats(const std::string& service,
                      const std::string& interface,
                      const std::string& method,
                      bool add_stat) {
    DCHECK_EQ(origin_thread_id_, base::PlatformThread::CurrentId());

    StatKey key = {service, interface, method};
    auto it = stats_.find(key);
    if (it != stats_.end())
      return &(it->second);

    if (!add_stat)
      return nullptr;

    return &(stats_[key]);
  }

  StatMap& stats() { return stats_; }
  base::Time start_time() { return start_time_; }

 private:
  StatMap stats_;
  base::Time start_time_;
  base::PlatformThreadId origin_thread_id_;
};

DBusStatistics* g_dbus_statistics = nullptr;

}  // namespace

//------------------------------------------------------------------------------

namespace statistics {

void Initialize() {
  if (g_dbus_statistics)
    delete g_dbus_statistics;  // reset statistics
  g_dbus_statistics = new DBusStatistics();
}

void Shutdown() {
  delete g_dbus_statistics;
  g_dbus_statistics = nullptr;
}

void AddSentMethodCall(const std::string& service,
                       const std::string& interface,
                       const std::string& method) {
  if (!g_dbus_statistics)
    return;
  g_dbus_statistics->AddStat(
      service, interface, method, DBusStatistics::TYPE_SENT_METHOD_CALLS);
}

void AddReceivedSignal(const std::string& service,
                       const std::string& interface,
                       const std::string& method) {
  if (!g_dbus_statistics)
    return;
  g_dbus_statistics->AddStat(
      service, interface, method, DBusStatistics::TYPE_RECEIVED_SIGNALS);
}

void AddBlockingSentMethodCall(const std::string& service,
                               const std::string& interface,
                               const std::string& method) {
  if (!g_dbus_statistics)
    return;
  g_dbus_statistics->AddStat(
      service, interface, method,
      DBusStatistics::TYPE_SENT_BLOCKING_METHOD_CALLS);
}

// NOTE: If the output format is changed, be certain to change the test
// expectations as well.
std::string GetAsString(ShowInString show, FormatString format) {
  if (!g_dbus_statistics)
    return "DBusStatistics not initialized.";

  const StatMap& stats = g_dbus_statistics->stats();
  if (stats.empty())
    return "No DBus calls.";

  base::TimeDelta dtime = base::Time::Now() - g_dbus_statistics->start_time();
  int dminutes = dtime.InMinutes();
  dminutes = std::max(dminutes, 1);

  std::string result;
  int sent = 0, received = 0, sent_blocking = 0;
  // Stats are stored in order by service, then interface, then method.
  for (auto iter = stats.begin(); iter != stats.end();) {
    auto cur_iter = iter;
    auto next_iter = ++iter;
    const StatKey& stat_key = cur_iter->first;
    const StatValue& stat = cur_iter->second;
    sent += stat.sent_method_calls;
    received += stat.received_signals;
    sent_blocking += stat.sent_blocking_method_calls;
    // If this is not the last stat, and if the next stat matches the current
    // stat, continue.
    if (next_iter != stats.end() &&
        next_iter->first.service == stat_key.service &&
        (show < SHOW_INTERFACE ||
         next_iter->first.interface == stat_key.interface) &&
        (show < SHOW_METHOD || next_iter->first.method == stat_key.method))
      continue;

    if (!sent && !received && !sent_blocking)
        continue;  // No stats collected for this line, skip it and continue.

    // Add a line to the result and clear the counts.
    std::string line;
    if (show == SHOW_SERVICE) {
      line += stat_key.service;
    } else {
      // The interface usually includes the service so don't show both.
      line += stat_key.interface;
      if (show >= SHOW_METHOD)
        line += "." + stat_key.method;
    }
    line += base::StringPrintf(":");
    if (sent_blocking) {
      line += base::StringPrintf(" Sent (BLOCKING):");
      if (format == FORMAT_TOTALS)
        line += base::StringPrintf(" %d", sent_blocking);
      else if (format == FORMAT_PER_MINUTE)
        line += base::StringPrintf(" %d/min", sent_blocking / dminutes);
      else if (format == FORMAT_ALL)
        line += base::StringPrintf(" %d (%d/min)",
                                   sent_blocking, sent_blocking / dminutes);
    }
    if (sent) {
      line += base::StringPrintf(" Sent:");
      if (format == FORMAT_TOTALS)
        line += base::StringPrintf(" %d", sent);
      else if (format == FORMAT_PER_MINUTE)
        line += base::StringPrintf(" %d/min", sent / dminutes);
      else if (format == FORMAT_ALL)
        line += base::StringPrintf(" %d (%d/min)", sent, sent / dminutes);
    }
    if (received) {
      line += base::StringPrintf(" Received:");
      if (format == FORMAT_TOTALS)
        line += base::StringPrintf(" %d", received);
      else if (format == FORMAT_PER_MINUTE)
        line += base::StringPrintf(" %d/min", received / dminutes);
      else if (format == FORMAT_ALL)
        line += base::StringPrintf(
            " %d (%d/min)", received, received / dminutes);
    }
    result += line + "\n";
    sent = 0;
    sent_blocking = 0;
    received = 0;
  }
  return result;
}

namespace testing {

bool GetCalls(const std::string& service,
              const std::string& interface,
              const std::string& method,
              int* sent,
              int* received,
              int* blocking) {
  if (!g_dbus_statistics)
    return false;
  StatValue* stat =
      g_dbus_statistics->GetStats(service, interface, method, false);
  if (!stat)
    return false;
  *sent = stat->sent_method_calls;
  *received = stat->received_signals;
  *blocking = stat->sent_blocking_method_calls;
  return true;
}

}  // namespace testing

}  // namespace statistics
}  // namespace dbus
