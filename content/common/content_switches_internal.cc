// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_switches_internal.h"

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/debug/debugger.h"
#include "base/feature_list.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include <signal.h>
static void SigUSR1Handler(int signal) {}
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/windows_version.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_WIN)

std::wstring ToNativeString(std::string_view string) {
  return base::ASCIIToWide(string);
}

std::string FromNativeString(std::wstring_view string) {
  return base::WideToASCII(string);
}

#else  // BUILDFLAG(IS_WIN)

std::string ToNativeString(const std::string& string) {
  return string;
}

std::string FromNativeString(const std::string& string) {
  return string;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

bool IsPinchToZoomEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Enable pinch everywhere unless it's been explicitly disabled.
  return !command_line.HasSwitch(switches::kDisablePinch);
}

blink::mojom::V8CacheOptions GetV8CacheOptions() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string v8_cache_options =
      command_line.GetSwitchValueASCII(switches::kV8CacheOptions);
  if (v8_cache_options.empty())
    v8_cache_options = base::FieldTrialList::FindFullName("V8CacheOptions");
  if (v8_cache_options == "none") {
    return blink::mojom::V8CacheOptions::kNone;
  } else if (v8_cache_options == "code") {
    return blink::mojom::V8CacheOptions::kCode;
  } else {
    return blink::mojom::V8CacheOptions::kDefault;
  }
}

void WaitForDebugger(const std::string& label) {
#if BUILDFLAG(IS_WIN)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::string title = "Google Chrome";
#else   // BUILDFLAG(CHROMIUM_BRANDING)
  std::string title = "Chromium";
#endif  // BUILDFLAG(CHROMIUM_BRANDING)
  title += " ";
  title += label;  // makes attaching to process easier
  std::string message = label;
  message += " starting with pid: ";
  message += base::NumberToString(base::GetCurrentProcId());
  ::MessageBox(NULL, base::UTF8ToWide(message).c_str(),
               base::UTF8ToWide(title).c_str(), MB_OK | MB_SETFOREGROUND);
#elif BUILDFLAG(IS_POSIX)
#if BUILDFLAG(IS_ANDROID)
  LOG(ERROR) << label << " waiting for GDB.";
  // Wait 24 hours for a debugger to be attached to the current process.
  base::debug::WaitForDebugger(24 * 60 * 60, true);
#else
  // TODO(playmobil): In the long term, overriding this flag doesn't seem
  // right, either use our own flag or open a dialog we can use.
  // This is just to ease debugging in the interim.
  LOG(ERROR) << label << " (" << getpid()
             << ") paused waiting for debugger to attach. "
             << "Send SIGUSR1 to unpause.";
  // Install a signal handler so that pause can be woken.
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SigUSR1Handler;
  sigaction(SIGUSR1, &sa, nullptr);

  pause();
#endif  // BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX)
}

std::vector<std::string> FeaturesFromSwitch(
    const base::CommandLine& command_line,
    const char* switch_name) {
  using NativeString = base::CommandLine::StringType;
  using NativeStringView = base::CommandLine::StringViewType;

  std::vector<std::string> features;
  if (!command_line.HasSwitch(switch_name))
    return features;

  // Store prefix as native string to avoid conversions for every arg.
  // (No string copies for the args that don't match the prefix.)
  NativeString prefix =
      ToNativeString(base::StringPrintf("--%s=", switch_name));
  for (NativeStringView arg : command_line.argv()) {
    // Switch names are case insensitive on Windows, but base::CommandLine has
    // already made them lowercase when building argv().
    if (!base::StartsWith(arg, prefix, base::CompareCase::SENSITIVE)) {
      continue;
    }
    arg.remove_prefix(prefix.size());
    if (!base::IsStringASCII(arg)) {
      continue;
    }
    auto vals = base::SplitString(FromNativeString(NativeString(arg)), ",",
                                  base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    features.insert(features.end(), vals.begin(), vals.end());
  }
  return features;
}

} // namespace content
