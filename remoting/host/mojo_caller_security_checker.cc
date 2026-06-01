// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_caller_security_checker.h"

#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "remoting/host/base/process_util.h"

#if BUILDFLAG(IS_MAC)
#include <array>
#include <string_view>

#include "remoting/host/mac/constants_mac.h"
#include "remoting/host/mac/trust_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <wtsapi32.h>

#include "base/strings/utf_string_conversions.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/sid.h"
#include "remoting/host/win/trust_util.h"
#endif

namespace remoting {
namespace {

#if BUILDFLAG(IS_LINUX)
constexpr auto kAllowedCallerProgramNames =
    base::MakeFixedFlatSet<base::FilePath::StringViewType>({
        "remote-open-url",
        "remote-session-info",
        "remote-webauthn",
        "login-session-reporter",
    });
#elif BUILDFLAG(IS_WIN)
constexpr auto kAllowedCallerProgramNames =
    base::MakeFixedFlatSet<base::FilePath::StringViewType>({
        L"remote_open_url.exe",
        L"remote_webauthn.exe",
        L"remote_security_key.exe",
    });
#elif BUILDFLAG(IS_MAC)
// Can't use constexpr here since `kBundleId` is not a constexpr.
// remoting_me2me_host is the bundle executable, so its identifier is the bundle
// identifier. For other binaries, the identifier is just the name of the
// binary.
static const auto kAllowedIdentifiers =
    std::to_array<const std::string_view>({kBundleId, "remote_webauthn"});
#endif

#if BUILDFLAG(IS_WIN)
bool IsWinCallerUserSidValid(
    const named_mojo_ipc_server::ConnectionInfo& caller) {
  std::optional<base::win::AccessToken> current_token =
      base::win::AccessToken::FromCurrentProcess();
  if (!current_token.has_value()) {
    PLOG(ERROR) << "Failed to open current process token.";
    return false;
  }

  std::optional<base::win::Sid> expected_sid;

  // Verify the client's identity depending on the host service context:
  // - If running as `LocalSystem` (standard service/production mode), the
  //   host has the necessary TCB privileges (`SE_TCB_NAME`) to query the
  //   legitimate active session user's token via `WTSQueryUserToken`.
  // - If running as a standard user process (developer diagnostics/testing
  //   environments or developer unit tests), the host lacks TCB privileges
  //   to query other session tokens. In this case, the active remote session's
  //   owner is simply the current process owner itself, so we securely fall
  //   back to validating the client against the current host process token.
  if (current_token->User() ==
      base::win::Sid::FromKnownSid(base::win::WellKnownSid::kLocalSystem)) {
    // SYSTEM Mode: Retrieve the target Windows session owner's User SID.
    HANDLE session_token_raw = nullptr;
    if (!::WTSQueryUserToken(caller.session_id, &session_token_raw)) {
      PLOG(ERROR) << "WTSQueryUserToken failed for session ID "
                  << caller.session_id;
      return false;
    }
    base::win::ScopedHandle session_token_handle(session_token_raw);

    std::optional<base::win::AccessToken> session_token =
        base::win::AccessToken::FromToken(std::move(session_token_handle));
    if (!session_token.has_value()) {
      PLOG(ERROR) << "Failed to get access token from session token handle.";
      return false;
    }
    expected_sid = session_token->User();
  } else {
    // User/Developer Mode Fallback: Use current process User SID as the
    // expected value.
    expected_sid = current_token->User();
  }

  std::optional<base::win::AccessToken> client_token =
      base::win::AccessToken::FromProcess(caller.process.Handle());
  if (!client_token.has_value()) {
    PLOG(ERROR) << "Failed to open client process token for PID " << caller.pid;
    return false;
  }
  base::win::Sid client_sid = client_token->User();

  if (client_sid != *expected_sid) {
    LOG(ERROR) << "Client user SID ("
               << base::WideToUTF8(client_sid.ToSddlString().value_or(L""))
               << ") does not match expected user SID ("
               << base::WideToUTF8(expected_sid->ToSddlString().value_or(L""))
               << ")";
    return false;
  }

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

bool IsTrustedMojoEndpoint(
    const named_mojo_ipc_server::ConnectionInfo& caller) {
#if BUILDFLAG(IS_MAC)
  return IsProcessTrusted(caller.audit_token, kAllowedIdentifiers);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

  static base::NoDestructor<base::FilePath> current_process_image_path(
      GetProcessImagePath(base::GetCurrentProcId()));

  base::FilePath caller_process_image_path;
#if BUILDFLAG(IS_WIN)
  if (!caller.process.IsValid()) {
    LOG(ERROR) << "caller.process is invalid for PID " << caller.pid
               << ". Was include_peer_process_info set to true?";
    return false;
  }
  caller_process_image_path = GetProcessImagePath(caller.process);
#else
  // TODO: yuweih - see if it's possible to move away from PID-based security
  // checks for Linux, which might be susceptible to PID reuse attacks.
  caller_process_image_path = GetProcessImagePath(caller.pid);
#endif

  if (caller_process_image_path.empty()) {
    LOG(ERROR) << "Cannot resolve process image path for caller with PID "
               << caller.pid;
    return false;
  }
  if (caller_process_image_path == *current_process_image_path) {
    // IPCs initiated from the same binary should be allowed.
    return true;
  }
  if (caller_process_image_path.DirName() !=
      current_process_image_path->DirName()) {
    LOG(ERROR) << "Process image " << caller_process_image_path
               << " is not under " << current_process_image_path->DirName();
    return false;
  }
  base::FilePath::StringType program_name =
      caller_process_image_path.BaseName().value();
  if (!kAllowedCallerProgramNames.contains(program_name)) {
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
    // Linux binaries generated in out/Debug are underscore-separated. To make
    // debugging easier, we just check the name again with underscores replaced
    // with hyphens.
    std::string program_name_hyphenated;
    base::ReplaceChars(program_name, "_", "-", &program_name_hyphenated);
    if (kAllowedCallerProgramNames.contains(program_name_hyphenated)) {
      return true;
    }
#endif  // BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
    LOG(ERROR) << caller_process_image_path.BaseName()
               << " is not in the list of allowed caller programs.";
    return false;
  }
#if BUILDFLAG(IS_WIN)
  if (!IsBinaryTrusted(caller_process_image_path)) {
    return false;
  }
  return IsWinCallerUserSidValid(caller);
#else
  // Linux binaries are not code-signed, so we just return true.
  return true;
#endif

#else  // Unsupported platform
  NOTREACHED();
#endif
}

}  // namespace remoting
