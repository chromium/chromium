// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_WINDOWS_EVENT_LOGGER_H_
#define REMOTING_HOST_WIN_WINDOWS_EVENT_LOGGER_H_

#include <windows.h>

#include <string>
#include <vector>

namespace remoting {

class WindowsEventLogger {
 public:
  explicit WindowsEventLogger(const std::string& application_name);
  WindowsEventLogger(WindowsEventLogger&&);
  WindowsEventLogger& operator=(WindowsEventLogger&&);
  ~WindowsEventLogger();

  // Indicates whether the instance has successfully registered itself with the
  // Windows event log API.
  bool IsRegistered();

  // Logs the message specified by |event_id| using |type| for the category.
  // If there are more strings passed in via |strings| than there are
  // placeholders in the message string, then the additional strings will be
  // logged as extra fields in 'EventData'.
  bool Log(WORD type, DWORD event_id, const std::vector<std::string>& strings);

 private:
  // The handle of the application event log.
  HANDLE event_log_ = nullptr;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_WINDOWS_EVENT_LOGGER_H_
