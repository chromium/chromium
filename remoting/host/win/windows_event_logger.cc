// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/windows_event_logger.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "remoting/host/win/remoting_host_messages.h"

namespace remoting {

WindowsEventLogger::WindowsEventLogger(const std::string& application_name) {
  event_log_ = ::RegisterEventSourceW(
      nullptr, base::UTF8ToWide(application_name).c_str());
  if (event_log_ == nullptr) {
    PLOG(ERROR) << "Failed to register the event source: " << application_name;
  }
}

WindowsEventLogger::WindowsEventLogger(WindowsEventLogger&& other)
    : event_log_(other.event_log_) {
  other.event_log_ = nullptr;
}

WindowsEventLogger& WindowsEventLogger::operator=(WindowsEventLogger&& other) {
  if (this != &other) {
    event_log_ = other.event_log_;
    other.event_log_ = nullptr;
  }
  return *this;
}

WindowsEventLogger::~WindowsEventLogger() {
  if (event_log_ != nullptr) {
    ::DeregisterEventSource(event_log_);
    event_log_ = nullptr;
  }
}

bool WindowsEventLogger::IsRegistered() {
  return event_log_ != nullptr;
}

bool WindowsEventLogger::Log(WORD type,
                             DWORD event_id,
                             const std::vector<std::string>& strings) {
  DCHECK(event_log_);

  // ReportEventW() takes an array of raw string pointers. They should stay
  // valid for the duration of the call.
  std::vector<const WCHAR*> raw_strings(strings.size());
  std::vector<std::wstring> wide_strings(strings.size());
  for (size_t i = 0; i < strings.size(); ++i) {
    wide_strings[i] = base::UTF8ToWide(strings[i]);
    raw_strings[i] = wide_strings[i].c_str();
  }

  return ::ReportEventW(event_log_, type, HOST_CATEGORY, event_id, nullptr,
                        static_cast<WORD>(raw_strings.size()), 0,
                        &raw_strings[0], nullptr);
}

}  // namespace remoting
