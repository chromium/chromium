// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements a wrapper to run the virtual me2me session within a
// proper PAM session. It will generally be run as root and drop privileges to
// the specified user before running the me2me session script.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Usage: user-session start [--foreground] [--user user] [-- SCRIPT_ARGS...]
//
// Options:
//   --foreground  - Don't daemonize.
//   --user        - Create a session for the specified user. Required when
//                   running as root, not allowed when running as a normal user.
//   SCRIPT_ARGS   - Arguments following -- are passed to the script verbatim.

#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace {

// This is the file descriptor used for the Python session script to pass us
// messages during startup. It must be kept in sync with USER_SESSION_MESSAGE_FD
// in linux_me2me_host.py. It should be high enough that login scripts are
// unlikely to interfere with it, but is otherwise arbitrary.
const int kMessageFd = 202;

// This is the exit code the Python session script will use to signal that the
// user-session wrapper should restart instead of exiting. It must be kept in
// sync with RELAUNCH_EXIT_CODE in linux_me2me_host.py
const int kRelaunchExitCode = 41;

const char kPamName[] = "chrome-remote-desktop";
const char kScriptName[] = "chrome-remote-desktop";
const char kStartCommand[] = "start";
const char kForegroundFlag[] = "--foreground";
const char kUserFlag[] = "--user";
const char kExeSymlink[] = "/proc/self/exe";

// This template will be formatted by strftime and then used by mkstemp
const char kLogFileTemplate[] =
    "/tmp/chrome_remote_desktop_%Y%m%d_%H%M%S_XXXXXX";

// The filename for the latest log symlink.
constexpr char kLatestLogSymlink[] = "/tmp/chrome_remote_desktop.latest";

const char kUsageMessage[] =
    "This program is not intended to be run by end users. To configure Chrome\n"
    "Remote Desktop, please install the app from the Chrome Web Store:\n"
    "https://chrome.google.com/remotedesktop\n";

// A list of variable to pass through to the child environment. Should be kept
// in sync with remoting_user_session_wrapper.sh for testing.
const char* const kPassthroughVariables[] = {
    "GOOGLE_CLIENT_ID_REMOTING", "GOOGLE_CLIENT_ID_REMOTING_HOST",
    "GOOGLE_CLIENT_SECRET_REMOTING", "GOOGLE_CLIENT_SECRET_REMOTING_HOST",
    "CHROME_REMOTE_DESKTOP_HOST_EXTRA_PARAMS"};

// Holds the null-terminated path to this executable. This is obtained at
// startup, since it may be harder to obtain later. (E.g., Linux will append
// " (deleted)" if the file has been replaced by an update.)
char gExecutablePath[PATH_MAX] = {};

void PrintUsage() {
  std::fputs(kUsageMessage, stderr);
}

// Shell-escapes a single argument in a way that is compatible with various
// different shells. Returns nullopt when argument contains a newline, which
// can't be represented in a cross-shell fashion.
std::optional<std::string> ShellEscapeArgument(std::string_view argument) {
  std::string result;
  for (char character : argument) {
    // csh in particular doesn't provide a good way to handle this
    if (character == '\n') {
      return std::nullopt;
    }

    // Some shells ascribe special meaning to some escape sequences such as \t,
    // so don't escape any alphanumerics. (Also cuts down on verbosity.) This is
    // similar to the approach sudo takes.
    if (!((character >= '0' && character <= '9') ||
          (character >= 'A' && character <= 'Z') ||
          (character >= 'a' && character <= 'z') ||
          (character == '-' || character == '_'))) {
      result.push_back('\\');
    }
    result.push_back(character);
  }
  return result;
}

// PAM conversation function. Since the wrapper runs in a non-interactive
// context, log any messages, but return an error if asked to provide user
// input.
extern "C" int Converse(int num_messages,
                        const struct pam_message** messages,
                        struct pam_response** responses,
                        void* context) {
  bool failed = false;

  for (int i = 0; i < num_messages; ++i) {
    // This is correct for the PAM included with Linux, OS X, and BSD. However,
    // apparently Solaris and HP/UX require instead `&(*msg)[i]`. That is, they
    // disagree as to which level of indirection contains the array.
    const pam_message* message = messages[i];

    switch (message->msg_style) {
      case PAM_PROMPT_ECHO_OFF:
      case PAM_PROMPT_ECHO_ON:
        LOG(WARNING) << "PAM requested user input (unsupported): "
                     << (message->msg ? message->msg : "");
        failed = true;
        break;
      case PAM_TEXT_INFO:
        LOG(INFO) << "[PAM] " << (message->msg ? message->msg : "");
        break;
      case PAM_ERROR_MSG:
        // Error messages from PAM are not necessarily fatal to the operation,
        // as the module may be optional.
        LOG(WARNING) << "[PAM] " << (message->msg ? message->msg : "");
        break;
      default:
        LOG(WARNING) << "Encountered unknown PAM message style";
        failed = true;
        break;
    }
  }

  if (failed) {
    return PAM_CONV_ERR;
  }

  pam_response* response_list = static_cast<pam_response*>(
      std::calloc(num_messages, sizeof(*response_list)));

  if (response_list == nullptr) {
    return PAM_BUF_ERR;
  }

  *responses = response_list;
  return PAM_SUCCESS;
}

const struct pam_conv kPamConversation = {Converse, nullptr};

// Wrapper class for working with PAM and cleaning up in an RAII fashion
class PamHandle {
 public:
  // Attempts to initialize PAM transaction. Check the result with IsInitialized
  // before calling any other member functions.
  PamHandle(const char* service_name,
            const char* user,
            const struct pam_conv* pam_conversation) {
    last_return_code_ = pam_start(service_name, user, pam_conversation,
                                  &pam_handle_.AsEphemeralRawAddr());
    if (last_return_code_ != PAM_SUCCESS) {
      pam_handle_ = nullptr;
    }
  }

  PamHandle(const PamHandle&) = delete;
  PamHandle& operator=(const PamHandle&) = delete;

  // Terminates PAM transaction
  ~PamHandle() {
    if (pam_handle_ != nullptr) {
      pam_end(pam_handle_, last_return_code_);
    }
  }

  // Checks whether the PAM transaction was successfully initialized. Only call
  // other member functions if this returns true.
  bool IsInitialized() const { return pam_handle_ != nullptr; }

  // Performs account validation
  int AccountManagement(int flags) {
    return last_return_code_ = pam_acct_mgmt(pam_handle_, flags);
  }

  // Establishes or deletes PAM user credentials
  int SetCredentials(int flags) {
    return last_return_code_ = pam_setcred(pam_handle_, flags);
  }

  // Starts user session
  int OpenSession(int flags) {
    return last_return_code_ = pam_open_session(pam_handle_, flags);
  }

  // Ends user session
  int CloseSession(int flags) {
    return last_return_code_ = pam_close_session(pam_handle_, flags);
  }

  int SetItem(int item_type, const char* value) {
    return last_return_code_ = pam_set_item(pam_handle_, item_type, value);
  }

  // Returns the current username according to PAM. It is possible for PAM
  // modules to change this from the initial value passed to the constructor.
  std::optional<std::string> GetUser() {
    const char* user;
    last_return_code_ = pam_get_item(pam_handle_, PAM_USER,
                                     reinterpret_cast<const void**>(&user));
    if (last_return_code_ != PAM_SUCCESS || user == nullptr) {
      return std::nullopt;
    }
    return std::string(user);
  }

  // Sets a PAM environment variable.
  int PutEnv(std::string_view name, std::string_view value) {
    std::string name_value = base::StrCat({name, "=", value});
    return last_return_code_ = pam_putenv(pam_handle_, name_value.c_str());
  }

  // Obtains the list of environment variables provided by PAM modules.
  std::optional<base::EnvironmentMap> GetEnvironment() {
    char** environment = pam_getenvlist(pam_handle_);

    if (environment == nullptr) {
      return std::nullopt;
    }

    base::EnvironmentMap environment_map;

    for (char** variable = environment; *variable != nullptr; ++variable) {
      char* delimiter = std::strchr(*variable, '=');
      if (delimiter != nullptr) {
        environment_map[std::string(*variable, delimiter)] =
            std::string(delimiter + 1);
      }
      std::free(*variable);
    }
    std::free(environment);

    return environment_map;
  }

  // Returns a description of the given return code
  const char* ErrorString(int return_code) {
    return pam_strerror(pam_handle_, return_code);
  }

  // Logs a fatal error if return_code isn't PAM_SUCCESS
  void CheckReturnCode(int return_code, std::string_view what) {
    if (return_code != PAM_SUCCESS) {
      LOG(FATAL) << "[PAM] " << what << ": " << ErrorString(return_code);
    }
  }

 private:
  raw_ptr<pam_handle_t> pam_handle_ = nullptr;
  int last_return_code_ = PAM_SUCCESS;
};

// Initializes the gExecutablePath global to the location of the running
// executable. Should be called at program start.
void DetermineExecutablePath() {
  ssize_t path_size =
      readlink(kExeSymlink, gExecutablePath, std::size(gExecutablePath));
  PCHECK(path_size >= 0) << "Failed to determine executable location";
  CHECK(path_size < PATH_MAX) << "Executable path too long";
  gExecutablePath[path_size] = '\0';
  CHECK(gExecutablePath[0] == '/') << "Executable path not absolute";
}

// Returns the expected location of the session script based on the path to
// this executable.
std::string FindScriptPath() {
  return base::FilePath(gExecutablePath).DirName().Append(kScriptName).value();
}

// Execs the me2me script.
// This function is called after forking and dropping privileges. It never
// returns.
[[noreturn]] void ExecMe2MeScript(base::EnvironmentMap environment,
                                  const struct passwd* pwinfo,
                                  const std::vector<std::string>& script_args) {
  std::string login_shell = pwinfo->pw_shell;
  if (login_shell.empty()) {
    // According to "man 5 passwd", if the shell field is empty, it defaults to
    // "/bin/sh".
    login_shell = "/bin/sh";
  }

  // By convention, a login shell is signified by preceding the shell name in
  // argv[0] with a '-'.
  std::string shell_name = '-' + base::FilePath(login_shell).BaseName().value();

  std::optional<std::string> escaped_script_path =
      ShellEscapeArgument(FindScriptPath());
  CHECK(escaped_script_path) << "Could not escape script path";

  std::string shell_arg = *escaped_script_path + " --start --child-process";

  for (const std::string& arg : script_args) {
    std::optional<std::string> escaped_arg = ShellEscapeArgument(arg);
    CHECK(escaped_arg) << "Could not escape script argument";
    shell_arg += " ";
    shell_arg += *escaped_arg;
  }

  environment["USER"] = pwinfo->pw_name;
  environment["LOGNAME"] = pwinfo->pw_name;
  environment["HOME"] = pwinfo->pw_dir;
  environment["SHELL"] = login_shell;
  if (!environment.count("PATH")) {
    environment["PATH"] = "/bin:/usr/bin";
  }
  environment["CHROME_REMOTE_DESKTOP_SESSION"] = "1";

  std::vector<std::string> env_strings;
  for (const auto& env_var : environment) {
    env_strings.emplace_back(env_var.first + "=" + env_var.second);
  }

  std::vector<const char*> arg_ptrs = {shell_name.c_str(), "-c",
                                       shell_arg.c_str(), nullptr};
  std::vector<const char*> env_ptrs;
  env_ptrs.reserve(env_strings.size() + 1);
  for (const auto& env_string : env_strings) {
    env_ptrs.push_back(env_string.c_str());
  }
  env_ptrs.push_back(nullptr);

  execve(login_shell.c_str(), const_cast<char* const*>(arg_ptrs.data()),
         const_cast<char* const*>(env_ptrs.data()));
  PLOG(FATAL) << "Failed to exec login shell " << login_shell;
}

// Either |user| must be set when running as root, xor the real user ID must be
// properly set when running as a user.
void Relaunch(const std::optional<std::string>& user,
              const std::vector<std::string>& script_args) {
  CHECK(user.has_value() == (getuid() == 0));

  // Pass --foreground to continue using the same log file.
  std::vector<const char*> arg_ptrs = {gExecutablePath, kStartCommand,
                                       kForegroundFlag};
  if (user) {
    arg_ptrs.push_back(kUserFlag);
    arg_ptrs.push_back(user->c_str());
  }
  arg_ptrs.push_back("--");
  for (const std::string& arg : script_args) {
    arg_ptrs.push_back(arg.c_str());
  }
  arg_ptrs.push_back(nullptr);

  execv(gExecutablePath, const_cast<char* const*>(arg_ptrs.data()));
  PCHECK(false) << "Failed to exec self";
}

// Runs the me2me script in a PAM session. Exits the program on failure.
// If chown_log is true, the owner and group of the file associated with stdout
// will be changed to the target user. If match_uid is specified, this function
// will fail if the final user id does not match the one provided. If
// script_args is not empty, the contained arguments will be passed on to the
// me2me script.
//
// Returns: whether the session should be relaunched.
bool ExecuteSession(std::string user,
                    bool chown_log,
                    std::optional<uid_t> match_uid,
                    const std::vector<std::string>& script_args) {
  PamHandle pam_handle(kPamName, user.c_str(), &kPamConversation);
  CHECK(pam_handle.IsInitialized()) << "Failed to initialize PAM";

  // Since we're running setuid root, we don't want to risk any user-set
  // environment variables changing the behavior of PAM modules, so copy any
  // variables we explicitly want to preserve into the PAM session and then
  // clear the environment.
  for (const char* variable : kPassthroughVariables) {
    char* value = std::getenv(variable);
    if (value != nullptr) {
      pam_handle.CheckReturnCode(pam_handle.PutEnv(variable, value),
                                 "Environment passthrough");
    }
  }
  clearenv();

  // Set various session attributes.
  pam_handle.CheckReturnCode(pam_handle.PutEnv("XDG_SESSION_CLASS", "user"),
                             "Set session class");
  pam_handle.CheckReturnCode(pam_handle.PutEnv("XDG_SESSION_TYPE", "x11"),
                             "Set session type");
  // Ideally, the TTY should be set to the X display for x11 sessions, but we
  // don't yet know what display we'll be using. Apparently some PAM modules
  // (the pam_systemd documentation explicitly calls out pam_time and
  // pam_access) require PAM_TTY to be set, so we set it to a dummy value. There
  // is some precedence for this, as SSH and cron set PAM_TTY to "ssh" and
  // "cron" (respectively) for similar reasons.
  // TODO(rkjnsn): This will prevent any PAM modules from, e.g., setting
  // session-related X properties. It would be more correct to implement a two-
  // phase session setup: first creating a "background/unspecified" session to
  // run the me2me script and the X server, and then launching a "user/x11"
  // session with PAM_TTY and PAM_XDISPLAY properly set to run the session
  // chooser or the user's session script. This would also allow the inner
  // session to be completely cleaned-up when the user selects "Logout" from
  // within their chromoting session.
  pam_handle.CheckReturnCode(
      pam_handle.SetItem(PAM_TTY, "chrome-remote-desktop"), "Set PAM_TTY");

  // Make sure the account is valid and enabled.
  pam_handle.CheckReturnCode(pam_handle.AccountManagement(0), "Account check");

  // PAM may remap the user at any stage.
  user = pam_handle.GetUser().value_or(std::move(user));

  // setcred explicitly does not handle user id or group membership, and
  // specifies that they should be established before calling setcred. Only the
  // real user id is set here, as we still require root privileges. PAM modules
  // may use getpwnam, so pwinfo can only be assumed valid until the next PAM
  // call.
  errno = 0;
  struct passwd* pwinfo = getpwnam(user.c_str());
  PCHECK(pwinfo != nullptr) << "getpwnam failed";
  PCHECK(setreuid(pwinfo->pw_uid, -1) == 0) << "setreuid failed";
  PCHECK(setgid(pwinfo->pw_gid) == 0) << "setgid failed";
  PCHECK(initgroups(pwinfo->pw_name, pwinfo->pw_gid) == 0)
      << "initgroups failed";

  // The documentation states that setcred should be called before open_session,
  // as done here, but it may be worth noting that `login` calls open_session
  // first.
  pam_handle.CheckReturnCode(pam_handle.SetCredentials(PAM_ESTABLISH_CRED),
                             "Set credentials");

  pam_handle.CheckReturnCode(pam_handle.OpenSession(0), "Open session");

  // The above may have remapped the user.
  user = pam_handle.GetUser().value_or(std::move(user));

  // Fetch pwinfo again, as it may have been invalidated or the user name might
  // have been remapped.
  pwinfo = getpwnam(user.c_str());
  PCHECK(pwinfo != nullptr) << "getpwnam failed";

  if (match_uid && pwinfo->pw_uid != *match_uid) {
    LOG(FATAL) << "PAM remapped username to one with a different user ID.";
  }

  if (chown_log) {
    int result = fchown(STDOUT_FILENO, pwinfo->pw_uid, pwinfo->pw_gid);
    PLOG_IF(WARNING, result != 0) << "Failed to change log file owner";
    result = lchown(kLatestLogSymlink, pwinfo->pw_uid, pwinfo->pw_gid);
    PLOG_IF(WARNING, result != 0)
        << "Failed to change latest log symlink owner";
  }

  pid_t child_pid = fork();
  PCHECK(child_pid >= 0) << "fork failed";
  if (child_pid == 0) {
    PCHECK(setuid(pwinfo->pw_uid) == 0) << "setuid failed";
    PCHECK(chdir(pwinfo->pw_dir) == 0) << "chdir to $HOME failed";
    std::optional<base::EnvironmentMap> pam_environment =
        pam_handle.GetEnvironment();
    CHECK(pam_environment) << "Failed to get environment from PAM";

    // Never returns.
    ExecMe2MeScript(std::move(*pam_environment), pwinfo, script_args);
  } else {
    // Close pipe write fd if it is open.
    close(kMessageFd);
    // waitpid will return if the child is ptraced, so loop until the process
    // actually exits.
    int status;
    do {
      pid_t wait_result = waitpid(child_pid, &status, 0);

      // Die if wait fails so we don't close the PAM session while the child is
      // still running.
      PCHECK(wait_result >= 0) << "wait failed";
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    bool relaunch = false;

    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status) == EXIT_SUCCESS) {
        LOG(INFO) << "Child exited successfully";
      } else if (WEXITSTATUS(status) == kRelaunchExitCode) {
        LOG(INFO) << "Restarting session";
        relaunch = true;
      } else {
        LOG(WARNING) << "Child exited with status " << WEXITSTATUS(status);
      }
    } else if (WIFSIGNALED(status)) {
      LOG(WARNING) << "Child terminated by signal " << WTERMSIG(status);
    }

    // Best effort PAM cleanup
    if (pam_handle.CloseSession(0) != PAM_SUCCESS) {
      LOG(WARNING) << "Failed to close PAM session";
    }
    std::ignore = pam_handle.SetCredentials(PAM_DELETE_CRED);

    return relaunch;
  }
}

struct LogFile {
  int fd;
  std::string path;
};

// Opens a temp file for logging. Exits the program on failure.
// Returns open file descriptor and path to log file.
LogFile OpenLogFile() {
  char logfile[265];
  std::time_t time = std::time(nullptr);
  CHECK_NE(time, (std::time_t)(-1));
  // Safe because we're single threaded
  std::tm* localtime = std::localtime(&time);
  CHECK_NE(std::strftime(logfile, sizeof(logfile), kLogFileTemplate, localtime),
           static_cast<std::size_t>(0))
      << "Failed to format log file name";

  mode_t mode = umask(0177);
  int fd = mkstemp(logfile);
  PCHECK(fd != -1) << "Failed to open log file";

  // Creates a symlink to make the logs easier to find.
  int symlink_ret = symlink(logfile, kLatestLogSymlink);
  if (symlink_ret != 0 && errno == EEXIST) {
    unlink(kLatestLogSymlink);
    symlink_ret = symlink(logfile, kLatestLogSymlink);
  }
  PLOG_IF(ERROR, symlink_ret != 0)
      << "Failed to create log symlink to " << logfile;

  umask(mode);

  return {fd, logfile};
}

// Find the username for the current user. If either USER or LOGNAME is set to
// a user matching our real user id, we return that. Otherwise, we use getpwuid
// to attempt a reverse lookup. Note: It's possible for multiple usernames to
// share the same user id (e.g., to allow a user to have logins with different
// home directories or group membership, but be considered the same user as far
// as file permissions are concerned). Consulting USER/LOGNAME allows us to pick
// the correct entry in these circumstances.
std::string FindCurrentUsername() {
  uid_t real_uid = getuid();
  struct passwd* pwinfo;
  for (const char* var : {"USER", "LOGNAME"}) {
    const char* value = getenv(var);
    if (value) {
      pwinfo = getpwnam(value);
      // USER and LOGNAME can be overridden, so make sure the value is valid
      // and matches the UID of the invoking user.
      if (pwinfo && pwinfo->pw_uid == real_uid) {
        return pwinfo->pw_name;
      }
    }
  }
  errno = 0;
  pwinfo = getpwuid(real_uid);
  PCHECK(pwinfo) << "getpwuid failed";
  return pwinfo->pw_name;
}

// Handle SIGINT and SIGTERM by printing a message and reraising the signal.
// This handler expects to be registered with the SA_RESETHAND and SA_NODEFER
// options to sigaction. (Don't register using signal.)
void HandleInterrupt(int signal) {
  static const char kInterruptedMessage[] =
      "Interrupted. The daemon is still running in the background.\n";
  // Use write since fputs isn't async-signal-handler safe.
  std::ignore = write(STDERR_FILENO, kInterruptedMessage,
                      std::size(kInterruptedMessage) - 1);
  raise(signal);
}

// Handle SIGALRM timeout
void HandleAlarm(int) {
  static const char kTimeoutMessage[] =
      "Timeout waiting for session to start. It may have crashed, or may still "
      "be running in the background.\n";
  // Use write since fputs isn't async-signal-handler safe.
  std::ignore =
      write(STDERR_FILENO, kTimeoutMessage, std::size(kTimeoutMessage) - 1);
  // A slow system or directory replication delay may cause the host to take
  // longer than expected to start. Since it may still succeed, optimistically
  // return success to prevent the host from being automatically unregistered.
  std::_Exit(EXIT_SUCCESS);
}

// Relay messages from the host session and then exit.
void WaitForMessagesAndExit(int read_fd, const std::string& log_name) {
  // Use initializer-list syntax to avoid trailing null
  static const std::string_view kMessagePrefix = "MSG:";
  static const std::string_view kReady = "READY\n";

  struct sigaction action = {};
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_RESETHAND | SA_NODEFER;

  // If Ctrl-C is pressed or TERM is received, inform the user that the daemon
  // is still running before exiting.
  action.sa_handler = HandleInterrupt;
  sigaction(SIGINT, &action, nullptr);
  sigaction(SIGTERM, &action, nullptr);

  // Install a fallback timeout to end the parent process, in case the daemon
  // never responds (e.g. host crash-looping, daemon killed).
  //
  // The value of 120s is chosen to match the heartbeat retry timeout in
  // hearbeat_sender.cc.
  action.sa_handler = HandleAlarm;
  sigaction(SIGALRM, &action, nullptr);
  alarm(120);

  std::FILE* stream = fdopen(read_fd, "r");
  char* buffer = nullptr;
  std::size_t buffer_size = 0;
  ssize_t line_size;
  bool message_received = false;
  bool host_ready = false;
  while ((line_size = getline(&buffer, &buffer_size, stream)) >= 0) {
    message_received = true;
    std::string_view line(buffer, line_size);
    if (base::StartsWith(line, kMessagePrefix, base::CompareCase::SENSITIVE)) {
      line.remove_prefix(kMessagePrefix.size());
      std::fwrite(line.data(), sizeof(char), line.size(), stderr);
    } else if (line == kReady) {
      host_ready = true;
    } else {
      std::fputs("Unrecognized command: ", stderr);
      std::fwrite(line.data(), sizeof(char), line.size(), stderr);
    }
  }

  // If we're not at EOF, it means a read error occured and we don't know if the
  // host is still running or not. Similarly, if we received an EOF before any
  // messages were received, it probably means the user's log-in shell closed
  // the pipe before execing the python script, so again we don't know the state
  // of the host. This latter behavior has only been observed in csh and tcsh.
  // All other shells tested allowed the python script to inherit the pipe file
  // descriptor without trouble.
  if (!std::feof(stream) || !message_received) {
    LOG(WARNING) << "Failed to read from message pipe. Please check log to "
                    "determine host status.\n";
    // Assume host started so native messaging host allows flow to complete.
    host_ready = true;
  }

  std::fprintf(stderr, "Log file: %s\n", log_name.c_str());

  std::exit(host_ready ? EXIT_SUCCESS : EXIT_FAILURE);
}

// Daemonizes the process. Output is redirected to a log file. Exits the program
// on failure. Only returns in the child process.
//
// When executed by root (almost certainly via the init script), or if a pipe
// cannot be created, the parent will immediately exit. When executed by a
// user, the parent process will drop privileges and wait for the host to
// start, relaying any start-up messages to stdout.
//
// TODO(lambroslambrou): Having stdout/stderr redirected to a log file is not
// ideal - it could create a filesystem DoS if the daemon or a child process
// were to write excessive amounts to stdout/stderr.  Ideally, stdout/stderr
// should be redirected to a pipe or socket, and a process at the other end
// should consume the data and write it to a logging facility which can do
// data-capping or log-rotation. The 'logger' command-line utility could be
// used for this, but it might cause too much syslog spam.
void Daemonize() {
  // Open file descriptors before forking so errors can be reported.
  LogFile log_file = OpenLogFile();
  int devnull_fd = open("/dev/null", O_RDONLY);
  PCHECK(devnull_fd != -1) << "Failed to open /dev/null";

  uid_t real_uid = getuid();

  // Set up message pipe
  bool pipe_created = false;
  int read_fd;
  if (real_uid != 0) {
    int pipe_fd[2];
    int pipe_result = ::pipe(pipe_fd);
    if (pipe_result != 0 || dup2(pipe_fd[1], kMessageFd) != kMessageFd) {
      PLOG(WARNING) << "Failed to create message pipe. Please check log to "
                       "determine host status.\n";
    } else {
      pipe_created = true;
      read_fd = pipe_fd[0];
      close(pipe_fd[1]);
    }
  }

  // Allow parent to exit, and ensure we're not a session leader so setsid can
  // succeed
  pid_t pid = fork();
  PCHECK(pid != -1) << "fork failed";

  if (pid != 0) {
    if (!pipe_created) {
      std::exit(EXIT_SUCCESS);
    } else {
      PCHECK(setuid(real_uid) == 0) << "setuid failed";
      close(kMessageFd);
      WaitForMessagesAndExit(read_fd, log_file.path);
      CHECK(false);
    }
  }

  // Start a new process group and session with no controlling terminal.
  PCHECK(setsid() != -1) << "setsid failed";

  // Fork again so we're no longer a session leader and can't get a controlling
  // terminal.
  pid = fork();
  PCHECK(pid != -1) << "fork failed";

  if (pid != 0) {
    std::exit(EXIT_SUCCESS);
  }

  LOG(INFO) << "Daemon process started in the background, logging to '"
            << log_file.path << "'";

  // We don't want to change to the target user's home directory until we've
  // dropped privileges, so change to / to make sure we're not keeping any other
  // directory in use.
  PCHECK(chdir("/") == 0) << "chdir / failed";

  PCHECK(dup2(devnull_fd, STDIN_FILENO) != -1) << "dup2 failed";
  PCHECK(dup2(log_file.fd, STDOUT_FILENO) != -1) << "dup2 failed";
  PCHECK(dup2(log_file.fd, STDERR_FILENO) != -1) << "dup2 failed";

  // Close all file descriptors except stdio and kMessageFd, including any we
  // may have inherited.
  if (pipe_created) {
    base::CloseSuperfluousFds(
        {base::InjectionArc(kMessageFd, kMessageFd, false)});
  } else {
    base::CloseSuperfluousFds(base::InjectiveMultimap());
  }
}

}  // namespace

int main(int argc, char** argv) {
  // Initialize gExecutablePath
  DetermineExecutablePath();

  // This binary requires elevated privileges.
  if (geteuid() != 0) {
    std::fprintf(stderr,
                 "%s not installed setuid root. Host must be started by "
                 "administrator.\n",
                 gExecutablePath);
    std::exit(EXIT_FAILURE);
  }

  if (argc < 2 || std::strcmp(argv[1], kStartCommand) != 0) {
    PrintUsage();
    std::exit(EXIT_FAILURE);
  }

  // Skip initial args
  argc -= 2;
  argv += 2;

  bool foreground = false;
  std::optional<std::string> user;
  std::vector<std::string> script_args;

  while (argc > 0) {
    if (std::strcmp(argv[0], kForegroundFlag) == 0) {
      foreground = true;
      argc -= 1;
      argv += 1;
    } else if (std::strcmp(argv[0], kUserFlag) == 0 && argc >= 2) {
      user = std::string(argv[1]);
      argc -= 2;
      argv += 2;
    } else if (std::strcmp(argv[0], "--") == 0) {
      argc -= 1;
      argv += 1;
      // Remaining args get forwarded to python script.
      while (argc > 0) {
        script_args.emplace_back(argv[0]);
        argc -= 1;
        argv += 1;
      }
    } else {
      PrintUsage();
      std::exit(EXIT_FAILURE);
    }
  }

  uid_t real_uid = getuid();

  // Note: This logic is security sensitive. It is imperative that a non-root
  // user is not allowed to specify an arbitrary target user.
  if (real_uid != 0) {
    if (user) {
      std::fputs("Target user may not be specified by non-root users.\n",
                 stderr);
      std::exit(EXIT_FAILURE);
    }
    user = FindCurrentUsername();
  } else {
    if (!user) {
      std::fputs("Target user must be specified when run as root.\n", stderr);
      std::exit(EXIT_FAILURE);
    }
  }

  if (!foreground) {
    Daemonize();
  }

  // Daemonizing redirects stdout to a log file, which we want to be owned by
  // the target user.
  bool chown_stdout = !foreground;
  std::optional<uid_t> match_uid =
      real_uid != 0 ? std::make_optional(real_uid) : std::nullopt;

  // Fork before opening PAM session so relaunches don't descend from the closed
  // PAM session.
  pid_t child_pid = fork();
  PCHECK(child_pid >= 0) << "fork failed";
  if (child_pid == 0) {
    bool relaunch = ExecuteSession(std::move(*user), chown_stdout, match_uid,
                                   std::move(script_args));
    std::exit(relaunch ? kRelaunchExitCode : EXIT_SUCCESS);
  } else {
    // Close pipe write fd if it is open.
    close(kMessageFd);
    // waitpid will return if the child is ptraced, so loop until the process
    // actually exits.
    int status;
    do {
      pid_t wait_result = waitpid(child_pid, &status, 0);

      // If we fail to wait on our child process, something has gone wrong and
      // there's not much we can do. Note that this means if the user later logs
      // out, the session won't restart.
      PCHECK(wait_result >= 0) << "wait failed";
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    if (WIFEXITED(status) && WEXITSTATUS(status) == kRelaunchExitCode) {
      // If running as root, forward the username argument to the relaunched
      // process. Otherwise, it should be inferred from the user id and
      // environment.
      Relaunch(real_uid == 0 ? user : std::nullopt, script_args);
    }
  }

  return EXIT_SUCCESS;
}
