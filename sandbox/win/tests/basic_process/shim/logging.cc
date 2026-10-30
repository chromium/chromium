// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/basic_process/shim/logging.h"

// clang-format off: Must precede C++ standard library headers.
#include "sandbox/win/tests/basic_process/shim/libcxx_support.h"
// clang-format on

#include <array>
#include <cstdint>
#include <string_view>

#include "sandbox/win/tests/basic_process/shim/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"
#include "sandbox/win/tests/basic_process/shim/string_util.h"

// Log file handle for file-based stub logging. Opened by DllEntryPoint from the
// explicit test-only log-file switch.
HANDLE g_log_file_handle = INVALID_HANDLE_VALUE;

// Opt-in call-site logging toggle (see logging.h / DllEntryPoint).
bool g_log_call_sites = false;

// Audit toggle: permits ORIGINAL_FN thunks to forward to the real OS API (see
// logging.h / DllEntryPoint). Off by default, so forwards trip a crash.
bool g_audit_basic_sandbox = false;

bool g_log_once_mode = false;

namespace basic_process {
namespace {

// Fixed-capacity line builder for the no-CRT shim.
template <size_t kCapacity>
class BoundedLine {
 public:
  BoundedLine() = default;
  BoundedLine(const BoundedLine&) = delete;
  BoundedLine& operator=(const BoundedLine&) = delete;
  void Append(const char* s) {
    if (s) {
      for (char c : std::string_view(s)) {
        AppendChar(static_cast<unsigned char>(c));
      }
    }
  }

  void AppendWide(const wchar_t* s) {
    if (s) {
      AppendWide(std::wstring_view(s));
    }
  }

  void AppendWide(std::wstring_view text) {
    if (buffer_.AppendPrefix(text) != text.size()) {
      truncated_ = true;
    }
  }

  void AppendChar(wchar_t c) {
    if (!buffer_.TryAppend(c)) {
      truncated_ = true;
    }
  }

  // Appends an unsigned decimal value.
  void AppendDecimal(uint64_t value) {
    std::array<wchar_t, kMaxUint64DecimalDigits> digits;
    AppendWide(FormatDecimal(value, digits));
  }

  // Appends "0x" followed by the minimal-width hex form of `value`.
  void AppendHex(uint64_t value) {
    std::array<wchar_t, kMaxUint64HexChars> text;
    AppendWide(FormatHex(value, text));
  }

  void AppendPointer(const void* p) {
    AppendHex(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)));
  }

  const wchar_t* CStr() const { return buffer_.c_str(); }

  size_t size() const { return buffer_.size(); }

  // Guarantees a newline and marks truncation, trimming partial stack frames.
  void Finish() {
    static constexpr char kTruncatedSuffix[] = " !truncated\n";
    constexpr size_t kSuffixLength = sizeof(kTruncatedSuffix) - 1;
    static_assert(kCapacity > kSuffixLength + 1);
    if (!truncated_ && buffer_.remaining()) {
      AppendChar(L'\n');
      return;
    }
    buffer_.Truncate(buffer_.capacity() - kSuffixLength);
    const std::wstring_view text = buffer_.view();
    const size_t separator = text.find_last_of(L";,");
    const size_t space = text.find_last_of(L' ');
    if (separator != std::wstring_view::npos &&
        (space == std::wstring_view::npos || separator > space)) {
      buffer_.Truncate(separator);
    }
    Append(kTruncatedSuffix);
  }

  // Writes the accumulated line to the shared --bp-log-file, if open. The file
  // format intentionally remains ASCII for existing report tooling.
  void WriteToLogFile() const {
    if (g_log_file_handle == INVALID_HANDLE_VALUE) {
      return;
    }
    std::array<char, kCapacity> bytes;
    const std::wstring_view text = buffer_.view();
    for (size_t i = 0; i < text.size(); ++i) {
      bytes[i] =
          (text[i] >= 0 && text[i] < 128) ? static_cast<char>(text[i]) : '?';
    }
    DWORD written = 0;
    ::WriteFile(g_log_file_handle, bytes.data(),
                static_cast<DWORD>(text.size()), &written, nullptr);
  }

 private:
  WideStringBuffer<kCapacity> buffer_;
  bool truncated_ = false;
};

using LogLine = BoundedLine<1024>;

// Stack records need room for module+RVA frames and loader detail.
using StackLogLine = BoundedLine<4096>;

// Dedup key builder. Small enough that a handful of unique keys fit; longer
// keys are truncated, which at worst merges two rare stubs in the dedup table.
using LogKey = BoundedLine<128>;

// Prepends the per-process line prefix "[<pid>] stub: ". The log file is
// shared by every child, so the PID keeps per-process API sets distinguishable
// within a single file.
void AppendPidPrefix(LogLine& line) {
  line.AppendChar('[');
  line.AppendDecimal(::GetCurrentProcessId());
  line.Append("] stub: ");
}

// Both sinks receive the same newline-terminated record.
template <typename Line>
void Emit(Line& line) {
  line.Finish();
  ::OutputDebugStringW(line.CStr());
  line.WriteToLogFile();
}

// Open-addressing dedup set. The half-full cap bounds probe cost.
constexpr int kLogOnceSlots = 1 << 17;
constexpr int kMaxLogOnceEntries = kLogOnceSlots / 2;
uint64_t g_log_once_table[kLogOnceSlots];
int g_log_once_count = 0;
// Ensures the saturation warning is emitted only once.
bool g_log_once_saturated = false;
CRITICAL_SECTION g_log_once_cs;
INIT_ONCE g_log_once_cs_init = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK InitLogOnceCs(PINIT_ONCE, PVOID, PVOID*) {
  ::InitializeCriticalSection(&g_log_once_cs);
  return TRUE;
}

// Zero marks an empty slot, so a zero hash is remapped.
uint64_t HashLogOnceKey(std::wstring_view key) {
  Fnv1aHasher hasher;
  hasher.AddString(key);
  return hasher.value() ? hasher.value() : 1;
}

// Returns true only when a new key is inserted below the cap.
bool LogOnceShouldEmit(const wchar_t* key) {
  const uint64_t hash = HashLogOnceKey(key);
  ::InitOnceExecuteOnce(&g_log_once_cs_init, InitLogOnceCs, nullptr, nullptr);
  ::EnterCriticalSection(&g_log_once_cs);
  bool should_emit = true;
  bool report_saturation = false;
  // SAFETY: every index is masked to [0, kLogOnceSlots). The load factor is
  // capped at one half, so probing always reaches an empty slot.
#pragma clang unsafe_buffer_usage begin
  int slot = static_cast<int>(hash & (kLogOnceSlots - 1));
  while (g_log_once_table[slot]) {
    if (g_log_once_table[slot] == hash) {
      should_emit = false;
      break;
    }
    slot = (slot + 1) & (kLogOnceSlots - 1);
  }
  if (should_emit) {
    if (g_log_once_count < kMaxLogOnceEntries) {
      g_log_once_table[slot] = hash;
      ++g_log_once_count;
    } else {
      should_emit = false;
      if (!g_log_once_saturated) {
        g_log_once_saturated = true;
        report_saturation = true;
      }
    }
  }
#pragma clang unsafe_buffer_usage end
  ::LeaveCriticalSection(&g_log_once_cs);

  // Keep I/O outside the lock and bypass dedup to avoid recursion.
  if (report_saturation) {
    LogLine line;
    AppendPidPrefix(line);
    line.Append("log-once table saturated");
    Emit(line);
  }
  return should_emit;
}

// Builds the dedup key "function|arg" (arg optional, narrow or wide).
void BuildDedupKey(LogKey& key,
                   const char* function,
                   const char* narrow_arg,
                   const wchar_t* wide_arg) {
  key.Append(function);
  key.AppendChar('|');
  if (narrow_arg) {
    key.Append(narrow_arg);
  } else if (wide_arg) {
    key.AppendWide(wide_arg);
  }
}

bool ShouldLogCall(const char* function,
                   const char* narrow_arg,
                   const wchar_t* wide_arg) {
  if (!g_log_once_mode) {
    return true;
  }
  LogKey key;
  BuildDedupKey(key, function, narrow_arg, wide_arg);
  return LogOnceShouldEmit(key.CStr());
}

// Bypasses dedup for tripwires and callers that already checked it.
void EmitFn(const char* function,
            const char* narrow_arg,
            const wchar_t* wide_arg) {
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  if (narrow_arg) {
    line.Append(narrow_arg);
  } else {
    line.AppendWide(wide_arg);
  }
  line.AppendChar(')');
  Emit(line);
}

// Emits module+RVA frames, omitting leading shim frames.
// SAFETY: `n` bounds frame access.
#pragma clang unsafe_buffer_usage begin
void AppendModuleFrames(StackLogLine& line, void* const* frames, int n) {
  const uintptr_t self_base = reinterpret_cast<uintptr_t>(g_module);
  bool wrote_any = false;
  bool past_shim = false;
  for (int i = 0; i < n; ++i) {
    HMODULE mod = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(frames[i]), &mod);
    const uintptr_t base = reinterpret_cast<uintptr_t>(mod);
    if (!past_shim) {
      if (base == self_base) {
        continue;
      }
      past_shim = true;
    }
    std::array<wchar_t, MAX_PATH> path;
    const DWORD pn = mod ? ::GetModuleFileNameW(mod, path.data(), MAX_PATH) : 0;
    if (wrote_any) {
      line.AppendChar(';');
    }
    if (pn) {
      // A truncated path returns the capacity, including its final NUL.
      const size_t length = pn < path.size() ? pn : path.size() - 1;
      const std::wstring_view text(path.begin(), path.end());
      line.AppendWide(BaseName(text.substr(0, length)));
    } else {
      line.AppendWide(L"?");
    }
    line.AppendChar('+');
    line.AppendHex(reinterpret_cast<uintptr_t>(frames[i]) - base);
    wrote_any = true;
  }
}
#pragma clang unsafe_buffer_usage end

// Keep the large buffer off the deduplicated hot path's stack.
__declspec(noinline) void EmitCallSite(const char* function,
                                       void* const* frames,
                                       int n,
                                       const char* detail_narrow,
                                       const wchar_t* detail_wide) {
  StackLogLine line;
  line.AppendChar('[');
  line.AppendDecimal(::GetCurrentProcessId());
  line.Append("] stub-site: ");
  line.Append(function);
  line.AppendChar(' ');
  AppendModuleFrames(line, frames, n);
  if (detail_narrow) {
    line.AppendChar(' ');
    line.Append(detail_narrow);
  } else if (detail_wide) {
    line.AppendChar(' ');
    line.AppendWide(detail_wide);
  }
  Emit(line);
}

constexpr int kMaxCallSiteFrames = 32;

// Hash process-local addresses before doing expensive module resolution.
// SAFETY: `n` bounds `frames`; detail strings are NUL-terminated.
#pragma clang unsafe_buffer_usage begin
uint64_t HashStack(void* const* frames,
                   int n,
                   const char* detail_narrow,
                   const wchar_t* detail_wide) {
  Fnv1aHasher hasher;
  for (int i = 0; i < n; ++i) {
    hasher.AddLittleEndian(reinterpret_cast<uintptr_t>(frames[i]));
  }
  if (detail_narrow) {
    hasher.AddString(detail_narrow);
  }
  if (detail_wide) {
    hasher.AddString(detail_wide);
  }
  return hasher.value();
}
#pragma clang unsafe_buffer_usage end

}  // namespace

void LogFn(const char* function, const char* arg) {
  if (ShouldLogCall(function, arg, nullptr)) {
    EmitFn(function, arg, nullptr);
  }
}

void LogFn(const char* function, const wchar_t* arg) {
  if (ShouldLogCall(function, nullptr, arg)) {
    EmitFn(function, nullptr, arg);
  }
}

void LogStackFn(const char* function) {
  // Tripwire records must bypass dedup.
  EmitFn(function, nullptr, nullptr);

  void* frames[24] = {};
  // Skip this function.
  WORD n = RtlCaptureStackBackTrace(1, 24, frames, nullptr);

  StackLogLine line;
  line.Append("stub-stack: ");
  line.Append(function);
  line.AppendChar(' ');
  AppendModuleFrames(line, frames, n);
  Emit(line);
}

void MaybeLogCallSite(const char* function,
                      void* return_address,
                      const char* detail_narrow,
                      const wchar_t* detail_wide) {
  // Skip this capture helper; AppendModuleFrames() drops remaining shim frames.
  void* frames[kMaxCallSiteFrames] = {};
  int n = RtlCaptureStackBackTrace(1, kMaxCallSiteFrames, frames, nullptr);
  if (n <= 0) {
    frames[0] = return_address;
    n = 1;
  }

  // Dedup on (function, whole stack, detail); resolve modules only for a
  // never-before-seen stack.
  LogKey key;
  key.Append(function);
  key.AppendChar('|');
  key.AppendHex(HashStack(frames, n, detail_narrow, detail_wide));
  if (!LogOnceShouldEmit(key.CStr())) {
    return;
  }
  EmitCallSite(function, frames, n, detail_narrow, detail_wide);
}

void LogFnOnceW(const char* function, const wchar_t* arg) {
  LogKey key;
  BuildDedupKey(key, function, nullptr, arg);
  if (LogOnceShouldEmit(key.CStr())) {
    EmitFn(function, nullptr, arg ? arg : L"");
  }
}

void LogFnOnceA(const char* function, const char* arg) {
  LogKey key;
  BuildDedupKey(key, function, arg, nullptr);
  if (LogOnceShouldEmit(key.CStr())) {
    EmitFn(function, arg ? arg : "", nullptr);
  }
}

void LogFnUInt(const char* function,
               const char* arg_name,
               unsigned int arg_value) {
  // Ignore varying values in log-once mode.
  if (!ShouldLogCall(function, arg_name, nullptr)) {
    return;
  }
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  line.Append(arg_name);
  line.AppendChar('=');
  line.AppendDecimal(arg_value);
  line.AppendChar(')');
  Emit(line);
}

void LogHandle(const char* function, const void* handle) {
  // Ignore varying handles in log-once mode.
  if (!ShouldLogCall(function, nullptr, nullptr)) {
    return;
  }
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  line.AppendPointer(handle);
  line.AppendChar(')');
  Emit(line);
}

void LogOnDemandLoad(const wchar_t* module_name) {
  if (g_log_file_handle == INVALID_HANDLE_VALUE) {
    return;
  }
  LogLine line;
  AppendPidPrefix(line);
  line.Append("on-demand loaded ");
  line.AppendWide(module_name);
  Emit(line);
}

}  // namespace basic_process
