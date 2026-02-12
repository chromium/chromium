// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pam_utils.h"

#include <security/pam_appl.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

int PamConversation(int num_messages,
                    const struct pam_message** messages,
                    struct pam_response** responses,
                    void* context) {
  // Assume we're only being asked to log messages, in which case our response
  // need to be free()-able zero-initialized memory.
  *responses = static_cast<struct pam_response*>(
      calloc(num_messages, sizeof(struct pam_response)));
  // SAFETY: `messages` is a pointer to a dynamically allocated array of
  // `pam_message`s, which should be at least `num_messages` long.
  // PamConversation is invoked as a callback from pam_start API, and is
  // documented to have this signature:
  // https://man7.org/linux/man-pages/man3/pam_start.3.html,
  // https://linux.die.net/man/3/pam_conv
  auto messages_span =
      UNSAFE_BUFFERS(base::span(messages, static_cast<size_t>(num_messages)));
  // We don't expect this function to be called. Since we have no easy way
  // of returning a response, we consider it to be an error if we're asked
  // for one and abort. Informational and error messages are logged.
  for (const pam_message* message : messages_span) {
    switch (message->msg_style) {
      case PAM_ERROR_MSG:
        LOG(ERROR) << "PAM conversation error message: " << message->msg;
        break;
      case PAM_TEXT_INFO:
        HOST_LOG << "PAM conversation message: " << message->msg;
        break;
      default:
        LOG(FATAL) << "Unexpected PAM conversation response required: "
                   << message->msg << "; msg_style = " << message->msg_style;
    }
  }
  return PAM_SUCCESS;
}

}  // namespace

bool IsLocalLoginAllowed(base::cstring_view username) {
  HOST_LOG << "Running local login check.";
  if (username.empty()) {
    LOG(ERROR) << "Username is empty.";
    return false;
  }
  struct pam_conv conv = {PamConversation, nullptr};
  pam_handle_t* handle = nullptr;
  HOST_LOG << "Calling pam_start() with username " << username;
  int result =
      pam_start("chrome-remote-desktop", username.c_str(), &conv, &handle);
  if (result != PAM_SUCCESS) {
    LOG(ERROR) << "pam_start() returned error " << result;
  } else {
    HOST_LOG << "Calling pam_acct_mgmt()";
    result = pam_acct_mgmt(handle, 0);
    if (result != PAM_SUCCESS) {
      LOG(ERROR) << "pam_acct_mgmt() returned error " << result;
    }
  }
  HOST_LOG << "Calling pam_end()";
  pam_end(handle, result);

  HOST_LOG << "Local login check for " << username
           << (result == PAM_SUCCESS ? " succeeded." : " failed.");

  return result == PAM_SUCCESS;
}

}  // namespace remoting
