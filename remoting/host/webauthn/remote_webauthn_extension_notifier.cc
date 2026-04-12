// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "remoting/host/webauthn/remote_webauthn_extension_notifier.h"

#include <optional>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/strings/string_util.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include <knownfolders.h>
#include <shlobj.h>
#include <wtsapi32.h>

#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#endif

namespace remoting {
namespace {

// Content of file doesn't matter so we just write an empty string.
static constexpr char kExtensionWakeupFileContent[] = "";

#if BUILDFLAG(IS_WIN)
// Helper class to impersonate a user and revert back when it goes out of scope.
// Note that Windows impersonation is bound to the current thread, so it is
// thread-safe.
class ScopedImpersonation {
 public:
  ScopedImpersonation(const ScopedImpersonation&) = delete;
  ScopedImpersonation& operator=(const ScopedImpersonation&) = delete;

  explicit ScopedImpersonation(HANDLE user_token) {
    if (user_token != nullptr && user_token != INVALID_HANDLE_VALUE) {
      if (ImpersonateLoggedOnUser(user_token)) {
        is_impersonating_ = true;
      } else {
        PLOG(ERROR) << "ImpersonateLoggedOnUser failed";
      }
    }
  }

  ~ScopedImpersonation() {
    if (is_impersonating_) {
      RevertToSelf();
    }
  }

  bool is_impersonating() const { return is_impersonating_; }

 private:
  bool is_impersonating_ = false;
};
#endif

}  // namespace

RemoteWebAuthnExtensionNotifier::RemoteStateChangeContext::
    RemoteStateChangeContext() = default;
RemoteWebAuthnExtensionNotifier::RemoteStateChangeContext::
    RemoteStateChangeContext(RemoteStateChangeContext&&) = default;
RemoteWebAuthnExtensionNotifier::RemoteStateChangeContext::
    ~RemoteStateChangeContext() = default;

// Returns a list of directories that different Chrome channels might use to
// watch for file changes for firing the onRemoteSessionStateChange event on the
// extension.
//
// The remote state change directory is located at:
//
//   REMOTE_STATE_CHANGE_DIRECTORY =
//     $DEFAULT_UDD/WebAuthenticationProxyRemoteSessionStateChange
//
// And the file for firing the event on the extension is located at:
//
//   $REMOTE_STATE_CHANGE_DIRECTORY/$EXTENSION_ID
//
// DEFAULT_UDD (default user data directory) is documented here:
//
//   https://chromium.googlesource.com/chromium/src/+/main/docs/user_data_dir.md#default-location
//
// Note that the default UDD is always used, and UDD overrides from browser
// launch args or env vars are ignored so the path is more discoverable by the
// native process.
//
// Caller should check if the directory exists before writing files to it. A
// directory only exists if the corresponding Chrome version is installed.
RemoteWebAuthnExtensionNotifier::RemoteStateChangeContext
RemoteWebAuthnExtensionNotifier::GetRemoteStateChangeContext() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  constexpr base::FilePath::CharType kStateChangeDirName[] =
      FILE_PATH_LITERAL("WebAuthenticationProxyRemoteSessionStateChange");
#endif

  RemoteWebAuthnExtensionNotifier::RemoteStateChangeContext context;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  std::vector<base::FilePath>& dirs = context.dirs;
#endif

#if BUILDFLAG(IS_LINUX)
  // See: chrome/common/chrome_paths_linux.cc
  auto env = base::Environment::Create();
  base::FilePath base_path;
  std::optional<std::string> chrome_config_home_str =
      env->GetVar("CHROME_CONFIG_HOME");
  if (chrome_config_home_str.has_value() &&
      base::IsStringUTF8(chrome_config_home_str.value())) {
    base_path = base::FilePath::FromUTF8Unsafe(chrome_config_home_str.value());
  } else {
    base_path = base::nix::GetXDGDirectory(
        env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
  }
  dirs.push_back(base_path.Append("google-chrome").Append(kStateChangeDirName));
  dirs.push_back(
      base_path.Append("google-chrome-beta").Append(kStateChangeDirName));
  dirs.push_back(
      base_path.Append("google-chrome-canary").Append(kStateChangeDirName));
  dirs.push_back(
      base_path.Append("google-chrome-unstable").Append(kStateChangeDirName));
  dirs.push_back(base_path.Append("chromium").Append(kStateChangeDirName));
#elif BUILDFLAG(IS_WIN)
  // See: chrome/common/chrome_paths_win.cc
  constexpr base::FilePath::CharType kUserDataDirName[] =
      FILE_PATH_LITERAL("User Data");

  // Get the LocalAppData path for the current logged in user. We can't just use
  // base::PathService since it returns LocalAppData for administrator on the
  // desktop process.
  HANDLE user_token = nullptr;
  if (!WTSQueryUserToken(WTS_CURRENT_SESSION, &user_token)) {
    PLOG(ERROR) << "Failed to get current user token";
    return context;
  }
  context.user_token.Set(user_token);
  base::win::ScopedCoMem<wchar_t> local_app_data_path_buf;
  if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, /* dwFlags= */ 0,
                                      context.user_token.get(),
                                      &local_app_data_path_buf))) {
    PLOG(ERROR) << "SHGetKnownFolderPath failed";
    return context;
  }

  base::FilePath base_path = base::FilePath(local_app_data_path_buf.get());
  base::FilePath base_path_google = base_path.Append(L"Google");
  dirs.push_back(base_path_google.Append(L"Chrome")
                     .Append(kUserDataDirName)
                     .Append(kStateChangeDirName));
  dirs.push_back(base_path_google.Append(L"Chrome Beta")
                     .Append(kUserDataDirName)
                     .Append(kStateChangeDirName));
  dirs.push_back(base_path_google.Append(L"Chrome Dev")
                     .Append(kUserDataDirName)
                     .Append(kStateChangeDirName));
  dirs.push_back(base_path_google.Append(L"Chrome SxS")
                     .Append(kUserDataDirName)
                     .Append(kStateChangeDirName));
  dirs.push_back(base_path.Append(L"Chromium")
                     .Append(kUserDataDirName)
                     .Append(kStateChangeDirName));
#elif BUILDFLAG(IS_MAC)
  // See: chrome/common/chrome_paths_mac.mm
  base::FilePath base_path;
  if (!base::PathService::Get(base::DIR_APP_DATA, &base_path)) {
    LOG(ERROR) << "Failed to get app data dir";
    return context;
  }
  base::FilePath base_path_google = base_path.Append("Google");
  dirs.push_back(base_path_google.Append("Chrome").Append(kStateChangeDirName));
  dirs.push_back(
      base_path_google.Append("Chrome Beta").Append(kStateChangeDirName));
  dirs.push_back(
      base_path_google.Append("Chrome Canary").Append(kStateChangeDirName));
  dirs.push_back(base_path.Append("Chromium").Append(kStateChangeDirName));
#else
  NOTIMPLEMENTED();
#endif
  return context;
}

// Core class for writing wakeup files on the IO sequence. Must be used and
// deleted on the same sequence.
class RemoteWebAuthnExtensionNotifier::Core final {
 public:
  explicit Core(RemoteStateChangeContext context);
  ~Core();

  void WakeUpExtension();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  RemoteStateChangeContext context_;
  base::WeakPtrFactory<Core> weak_factory_{this};
};

RemoteWebAuthnExtensionNotifier::Core::Core(RemoteStateChangeContext context)
    : context_(std::move(context)) {}

RemoteWebAuthnExtensionNotifier::Core::~Core() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RemoteWebAuthnExtensionNotifier::Core::WakeUpExtension() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_WIN)
  ScopedImpersonation impersonation(context_.user_token.get());
  if (context_.user_token.is_valid() && !impersonation.is_impersonating()) {
    PLOG(ERROR) << "Aborting file writes due to impersonation failure.";
    return;
  }
#endif

  for (const base::FilePath& dir : context_.dirs) {
    // Note: We check DirectoryExists as the user. If they've swapped the
    // directory for a junction to a protected area, the check will fail.
    if (!base::DirectoryExists(dir)) {
      VLOG(1) << "Ignored non-directory path: " << dir;
      continue;
    }
    for (const auto& id : GetRemoteWebAuthnExtensionIds()) {
      auto file_path = dir.Append(id);
      VLOG(1) << "Writing extension wakeup file: " << file_path;
      // We are impersonating the user, so any junction-based redirection will
      // be restricted by the user's permissions.
      base::File file(file_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
      if (!file.IsValid()) {
        // A file creation failure here is likely due to permission issues,
        // sharing violations, or the path being redirected via a junction to a
        // location where the impersonated user lacks write access.
        VLOG(1) << "Failed to open extension wakeup file: " << file_path
                << " error: " << file.error_details();
        continue;
      }
      file.WriteAtCurrentPos(
          base::byte_span_with_nul_from_cstring(kExtensionWakeupFileContent));
      file.Flush();
    }
  }
}

// static
const std::vector<base::FilePath::StringType>&
RemoteWebAuthnExtensionNotifier::GetRemoteWebAuthnExtensionIds() {
  static const base::NoDestructor<std::vector<base::FilePath::StringType>> ids({
      // LINT.IfChange(extension_ids)
      // Prod security key extension ID
      FILE_PATH_LITERAL("djjmngfglakhkhmgcfdmjalogilepkhd"),

      // Prod companion extension ID
      FILE_PATH_LITERAL("inomeogfingihgjfjlpeplalcfajhgai"),

  // For debug builds we wake up both extensions, so that developers don't
  // have to build and install the dev extension for using WebAuthn
  // forwarding.
#if !defined(NDEBUG)
      // Dev security key extension ID
      FILE_PATH_LITERAL("kbapnajlciffffomeaphfpckfdcfopef"),

      // Dev companion extension ID
      FILE_PATH_LITERAL("pbnaomcgbfiofkfobmlhmdobjchjkphi"),
#endif
      // LINT.ThenChange(/remoting/host/BUILD.gn:extension_ids)
  });
  return *ids;
}

RemoteWebAuthnExtensionNotifier::RemoteWebAuthnExtensionNotifier()
    : RemoteWebAuthnExtensionNotifier(
          GetRemoteStateChangeContext(),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

RemoteWebAuthnExtensionNotifier::RemoteWebAuthnExtensionNotifier(
    RemoteStateChangeContext context,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  core_ = base::SequenceBound<Core>(io_task_runner, std::move(context));
}

RemoteWebAuthnExtensionNotifier::~RemoteWebAuthnExtensionNotifier() {
  if (is_wake_up_scheduled_) {
    // The scheduled RemoteWebAuthnExtensionNotifier::WakeUpExtension will not
    // be executed since |weak_factory_| will soon be invalidated, so we just
    // async-call Core::WakeUpExtension here. It is scheduled before the
    // deletion of |core_| on the IO sequence, so it is guaranteed to be
    // executed.
    core_.AsyncCall(&Core::WakeUpExtension);
  }
}

void RemoteWebAuthnExtensionNotifier::NotifyStateChange() {
  // NotifyStateChange might be called multiple times in the same turn of the
  // message loop, so we deduplicate by posting a task to the current task
  // runner, which posts another to execute Core::WakeUpExtension on the IO
  // sequence.
  if (is_wake_up_scheduled_) {
    return;
  }
  is_wake_up_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteWebAuthnExtensionNotifier::WakeUpExtension,
                     weak_factory_.GetWeakPtr()));
}

void RemoteWebAuthnExtensionNotifier::WakeUpExtension() {
  is_wake_up_scheduled_ = false;

  core_.AsyncCall(&Core::WakeUpExtension);
}

}  // namespace remoting
