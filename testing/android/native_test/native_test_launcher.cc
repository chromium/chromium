// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This class sets up the environment for running the native tests inside an
// android application. It outputs (to a fifo) markers identifying the
// START/PASSED/CRASH of the test suite, FAILURE/SUCCESS of individual tests,
// etc.
// These markers are read by the test runner script to generate test results.
// It installs signal handlers to detect crashes.

#include <android/log.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include <iterator>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/clang_profiling_buildflags.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/test_support_android.h"
#include "base/threading/thread_restrictions.h"
#include "gtest/gtest.h"
#include "testing/android/native_test/main_runner.h"
#include "testing/android/native_test/native_test_jni/NativeTest_jni.h"
#include "testing/android/native_test/native_test_util.h"

#if BUILDFLAG(CLANG_PROFILING)
#include "base/test/clang_profiling.h"
#endif

#if defined(__ANDROID_CLANG_COVERAGE__)
// This is only used by Cronet in AOSP.
extern "C" int __llvm_profile_dump(void);
#endif

using jni_zero::JavaParamRef;

// The main function of the program to be wrapped as a test apk.
extern int main(int argc, char** argv);

namespace testing {
namespace android {

namespace {

const char kLogTag[] = "chromium";
const char kCrashedMarker[] = "[ CRASHED      ]\n";

// The list of signals which are considered to be crashes.
const int kExceptionSignals[] = {
  SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS, -1
};

struct sigaction g_old_sa[NSIG];

// This function runs in a compromised context. It should not allocate memory.
void SignalHandler(int sig, siginfo_t* info, void* reserved) {
  // Output the crash marker.
  write(STDOUT_FILENO, kCrashedMarker, sizeof(kCrashedMarker) - 1);
  g_old_sa[sig].sa_sigaction(sig, info, reserved);
}

// Writes printf() style string to Android's logger where |priority| is one of
// the levels defined in <android/log.h>.
void AndroidLog(int priority, const char* format, ...) {
  va_list args;
  va_start(args, format);
  __android_log_vprint(priority, kLogTag, format, args);
  va_end(args);
}

}  // namespace

static void JNI_NativeTest_RunTests(
    JNIEnv* env,
    const JavaParamRef<jstring>& jcommand_line_flags,
    const JavaParamRef<jstring>& jcommand_line_file_path,
    const JavaParamRef<jstring>& jstdout_file_path,
    const JavaParamRef<jobject>& app_context,
    const JavaParamRef<jstring>& jtest_data_dir) {
  base::ScopedAllowBlockingForTesting allow;

  // Required for DEATH_TESTS.
  pthread_atfork(nullptr, nullptr, jni_zero::DisableJvmForTesting);

  // Command line initialized basically, will be fully initialized later.
  static const char* const kInitialArgv[] = { "ChromeTestActivity" };
  base::CommandLine::Init(std::size(kInitialArgv), kInitialArgv);

  std::vector<std::string> args;

  const std::string command_line_file_path(
      base::android::ConvertJavaStringToUTF8(env, jcommand_line_file_path));
  if (command_line_file_path.empty())
    args.push_back("_");
  else
    ParseArgsFromCommandLineFile(command_line_file_path.c_str(), &args);

  const std::string command_line_flags(
      base::android::ConvertJavaStringToUTF8(env, jcommand_line_flags));
  ParseArgsFromString(command_line_flags, &args);

  std::vector<char*> argv;
  int argc = ArgsToArgv(args, &argv);

  // Fully initialize command line with arguments.
  base::CommandLine::ForCurrentProcess()->AppendArguments(
      base::CommandLine(argc, &argv[0]), false);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  base::FilePath stdout_file_path(
      base::android::ConvertJavaStringToUTF8(env, jstdout_file_path));

  // A few options, such "--gtest_list_tests", will just use printf directly
  // Always redirect stdout to a known file.
  if (freopen(stdout_file_path.value().c_str(), "a+", stdout) == NULL) {
    AndroidLog(ANDROID_LOG_ERROR, "Failed to redirect stream to file: %s: %s\n",
               stdout_file_path.value().c_str(), strerror(errno));
    exit(EXIT_FAILURE);
  }
  // TODO(jbudorick): Remove this after resolving crbug.com/726880
  AndroidLog(ANDROID_LOG_INFO, "Redirecting stdout to file: %s\n",
             stdout_file_path.value().c_str());
  dup2(STDOUT_FILENO, STDERR_FILENO);

  if (command_line.HasSwitch(switches::kWaitForDebugger)) {
    AndroidLog(ANDROID_LOG_VERBOSE,
               "Native test waiting for GDB because flag %s was supplied",
               switches::kWaitForDebugger);
    base::debug::WaitForDebugger(24 * 60 * 60, true);
  }

  base::FilePath test_data_dir(
      base::android::ConvertJavaStringToUTF8(env, jtest_data_dir));
  base::InitAndroidTestPaths(test_data_dir);

  ScopedMainEntryLogger scoped_main_entry_logger;
  main(argc, &argv[0]);

// Explicitly write profiling data to LLVM profile file.
#if BUILDFLAG(CLANG_PROFILING)
  base::WriteClangProfilingProfile();
#elif defined(__ANDROID_CLANG_COVERAGE__)
  // Cronet runs tests in AOSP, where due to build system constraints, compiler
  // flags can be changed (to enable coverage), but source files cannot be
  // conditionally linked (as is the case with `clang_profiling.cc`).
  //
  //  This will always get called from a single thread unlike
  //  base::WriteClangProfilingProfile hence the lack of locks.
  __llvm_profile_dump();
#endif
}

// TODO(nileshagrawal): now that we're using FIFO, test scripts can detect EOF.
// Remove the signal handlers.
void InstallHandlers() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));

  sa.sa_sigaction = SignalHandler;
  sa.sa_flags = SA_SIGINFO;

  for (unsigned int i = 0; kExceptionSignals[i] != -1; ++i) {
    sigaction(kExceptionSignals[i], &sa, &g_old_sa[kExceptionSignals[i]]);
  }
}

}  // namespace android
}  // namespace testing
