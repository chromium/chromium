// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_CHROMEOS_ENTERPRISE_PARAMS_H_
#define REMOTING_HOST_CHROMEOS_CHROMEOS_ENTERPRISE_PARAMS_H_

namespace remoting {

// ChromeOS enterprise specific parameters.
// These parameters are not exposed through the public Mojom APIs, for security
// reasons.
struct ChromeOsEnterpriseParams {
  // Local machine configuration.
  bool suppress_user_dialogs = false;
  bool suppress_notifications = false;
  bool terminate_upon_input = false;
  bool curtain_local_user_session = false;

  // Remote machine configuration.
  bool show_troubleshooting_tools = false;
  bool allow_troubleshooting_tools = false;
  bool allow_reconnections = false;
  bool allow_file_transfer = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_CHROMEOS_ENTERPRISE_PARAMS_H_
