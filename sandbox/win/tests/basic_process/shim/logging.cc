// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/tests/basic_process/shim/logging.h"

#include <cstdint>

#include "sandbox/win/tests/basic_process/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/shim_runtime.h"

// Log file handle for file-based stub logging. Opened by DllEntryPoint from the
// explicit test-only log-file switch.
HANDLE g_log_file_handle = INVALID_HANDLE_VALUE;

// Opt-in call-site logging toggle (see logging.h / DllEntryPoint).
bool g_log_call_sites = false;

// Audit toggle: permits ORIGINAL_FN thunks to forward to the real OS API (see
// logging.h / DllEntryPoint). Off by default, so forwards trip a crash.
bool g_audit_basic_sandbox = false;

namespace basic_process {
namespace {

// A fixed-capacity, bounds-checked line builder for composing one log line
// without the CRT or //base. Every append is capped at the buffer's own
// capacity, so no write can overflow; an over-long line is silently truncated
// (logging is best-effort). Co-locating the storage with its length — instead
// of threading a raw (dst, pos, max) triple through every helper and call
// site — confines the only raw-pointer bookkeeping, and the single scoped
// unsafe-buffer escape hatch, to this one small, auditable class. Callers hold
// a LogLine and call type-checked member functions, so they need no unsafe
// annotation of their own.
//
// SAFETY: the members below index the fixed array `buf_` solely by `len_`,
// which every method keeps in [0, kCapacity - 1] (one wchar_t is always
// reserved for the NUL that CStr() appends). //base is unavailable in this
// no-CRT DLL, so base::span cannot carry the bound; the pragma scopes the
// in-bounds raw indexing to this class.
#pragma clang unsafe_buffer_usage begin
template <int kCapacity>
class BoundedLine {
 public:
  BoundedLine() = default;
  BoundedLine(const BoundedLine&) = delete;
  BoundedLine& operator=(const BoundedLine&) = delete;
  void Append(const char* s) {
    for (; s && *s && len_ < kCapacity - 1; ++s) {
      buf_[len_++] = static_cast<unsigned char>(*s);
    }
  }

  void AppendWide(const wchar_t* s) {
    for (; s && *s && len_ < kCapacity - 1; ++s) {
      buf_[len_++] = *s;
    }
  }

  void AppendChar(wchar_t c) {
    if (len_ < kCapacity - 1) {
      buf_[len_++] = c;
    }
  }

  // Appends an unsigned decimal value.
  void AppendDecimal(uint64_t value) {
    wchar_t tmp[20];
    int n = 0;
    do {
      tmp[n++] = static_cast<wchar_t>(L'0' + value % 10);
      value /= 10;
    } while (value && n < static_cast<int>(sizeof(tmp) / sizeof(tmp[0])));
    while (n > 0) {
      AppendChar(tmp[--n]);
    }
  }

  // Appends "0x" followed by the minimal-width hex form of `value`.
  void AppendHex(uint64_t value) {
    static const wchar_t kHex[] = L"0123456789abcdef";
    Append("0x");
    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
      int nibble = static_cast<int>((value >> shift) & 0xF);
      if (nibble != 0 || started || shift == 0) {
        AppendChar(kHex[nibble]);
        started = true;
      }
    }
  }

  void AppendPointer(const void* p) {
    AppendHex(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(p)));
  }

  // Appends `count` frame pointers, comma-separated.
  void AppendFrames(void* const* frames, int count) {
    for (int i = 0; i < count; ++i) {
      AppendPointer(frames[i]);
      AppendChar(',');
    }
  }

  const wchar_t* CStr() {
    buf_[len_] = '\0';
    return buf_;
  }

  int size() const { return len_; }

  // Writes the accumulated line to the shared --bp-log-file, if open. The file
  // format intentionally remains ASCII for existing report tooling.
  void WriteToLogFile() const {
    if (g_log_file_handle == INVALID_HANDLE_VALUE) {
      return;
    }
    char bytes[kCapacity];
    for (int i = 0; i < len_; ++i) {
      bytes[i] =
          (buf_[i] >= 0 && buf_[i] < 128) ? static_cast<char>(buf_[i]) : '?';
    }
    DWORD written = 0;
    ::WriteFile(g_log_file_handle, bytes, static_cast<DWORD>(len_), &written,
                nullptr);
  }

 private:
  wchar_t buf_[kCapacity];
  int len_ = 0;
};
#pragma clang unsafe_buffer_usage end

using LogLine = BoundedLine<1024>;

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

// Routes one fully-composed line to both sinks: the debugger
// (OutputDebugString) and the shared --bp-log-file. Building the line once and
// emitting it here avoids constructing the same text twice and keeps the two
// sinks byte-for-byte identical.
void Emit(LogLine& line) {
  ::OutputDebugStringW(line.CStr());
  line.WriteToLogFile();
}

// Dedup table for "log once per unique (function, arg) pair" stubs, used by
// LoadLibrary* / GetModuleHandle* so we still record which modules were seen
// without flooding the log with repeats — and by the call-site logger, which
// keys on (function, module+rva). Lock-protected; a small linear scan is fine
// since the table grows slowly relative to total calls. Sized for the call-site
// case (a few sites per logged API); grows to a hard cap then stops recording
// new keys (existing keys keep deduping).
constexpr int kMaxLogOnceEntries = 4096;
struct LogOnceEntry {
  wchar_t key[128];
};
LogOnceEntry g_log_once_table[kMaxLogOnceEntries];
int g_log_once_count = 0;
CRITICAL_SECTION g_log_once_cs;
INIT_ONCE g_log_once_cs_init = INIT_ONCE_STATIC_INIT;

BOOL CALLBACK InitLogOnceCs(PINIT_ONCE, PVOID, PVOID*) {
  ::InitializeCriticalSection(&g_log_once_cs);
  return TRUE;
}

// Atomically checks the table for `key` and inserts it if absent. Returns true
// if this is the first time `key` was seen (caller should log), false for a
// duplicate (caller should skip).
bool LogOnceShouldEmit(const wchar_t* key) {
  ::InitOnceExecuteOnce(&g_log_once_cs_init, InitLogOnceCs, nullptr, nullptr);
  ::EnterCriticalSection(&g_log_once_cs);
  bool should_emit = true;
  // SAFETY: `i` and `g_log_once_count` are both kept in
  // [0, kMaxLogOnceEntries], so every g_log_once_table[] subscript below is in
  // bounds; the copy is additionally capped at the 128-byte slot width.
#pragma clang unsafe_buffer_usage begin
  for (int i = 0; i < g_log_once_count; ++i) {
    const wchar_t* lhs = g_log_once_table[i].key;
    const wchar_t* rhs = key;
    while (*lhs && *lhs == *rhs) {
      ++lhs;
      ++rhs;
    }
    if (*lhs == *rhs) {
      should_emit = false;
      break;
    }
  }
  if (should_emit) {
    if (g_log_once_count < kMaxLogOnceEntries) {
      wchar_t* dst = g_log_once_table[g_log_once_count].key;
      int j = 0;
      while (key[j] && j < 127) {
        dst[j] = key[j];
        ++j;
      }
      dst[j] = '\0';
      ++g_log_once_count;
    } else {
      should_emit = false;
    }
  }
#pragma clang unsafe_buffer_usage end
  ::LeaveCriticalSection(&g_log_once_cs);
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

}  // namespace

void LogFn(const char* function, const char* arg) {
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  line.Append(arg);
  line.Append(")\n");
  Emit(line);
}

void LogFn(const char* function, const wchar_t* arg) {
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  line.AppendWide(arg);
  line.Append(")\n");
  Emit(line);
}

void LogStackFn(const char* function) {
  // Emit the normal "stub:" line first so the call still shows up in the
  // per-API histogram.
  LogFn(function);

  void* frames[24] = {};
  // Skip frame 0 (this function) so the first logged frame is the stub.
  WORD n = RtlCaptureStackBackTrace(1, 24, frames, nullptr);

  LogLine line;
  line.Append("stub-stack: ");
  line.Append(function);
  line.AppendChar(' ');
  line.AppendFrames(frames, n);
  line.AppendChar('\n');
  Emit(line);
}

// Maximum stack depth captured per call site. Deep enough to reach meaningful
// application frames; stacks are deduplicated by full contents, so log volume
// stays bounded regardless of call frequency.
constexpr int kMaxCallSiteFrames = 32;

// FNV-1a hash of the raw frame addresses plus the detail string, used to dedup
// stacks per process. Absolute addresses are process-stable and
// LogOnceShouldEmit is per-process, so hashing raw pointers is sound and lets
// the hot (already-seen) path skip all module resolution.
// SAFETY: `frames[i]` is read only for i in [0, n); `n` is the frame count from
// RtlCaptureStackBackTrace (<= kMaxCallSiteFrames). //base is unavailable here.
#pragma clang unsafe_buffer_usage begin
static uint64_t HashStack(void* const* frames,
                          int n,
                          const char* detail_narrow,
                          const wchar_t* detail_wide) {
  uint64_t h = 1469598103934665603ULL;
  auto mix_byte = [&h](unsigned char b) {
    h ^= b;
    h *= 1099511628211ULL;
  };
  for (int i = 0; i < n; ++i) {
    uint64_t v = reinterpret_cast<uintptr_t>(frames[i]);
    for (int b = 0; b < 8; ++b) {
      mix_byte(static_cast<unsigned char>((v >> (b * 8)) & 0xFF));
    }
  }
  for (const char* p = detail_narrow; p && *p; ++p) {
    mix_byte(static_cast<unsigned char>(*p));
  }
  for (const wchar_t* p = detail_wide; p && *p; ++p) {
    mix_byte(static_cast<unsigned char>(*p & 0xFF));
  }
  return h;
}
#pragma clang unsafe_buffer_usage end

void MaybeLogCallSite(const char* function,
                      void* return_address,
                      const char* detail_narrow,
                      const wchar_t* detail_wide) {
  // Capture the caller stack. Skip frame 0 (MaybeLogCallSite itself); the
  // remaining apifw.dll frames (the Apifw* thunk) are dropped by module below.
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

  const uintptr_t self_base = reinterpret_cast<uintptr_t>(g_module);
  LogLine line;
  line.AppendChar('[');
  line.AppendDecimal(::GetCurrentProcessId());
  line.Append("] stub-site: ");
  line.Append(function);
  line.AppendChar(' ');

  // Emit the stack as ";"-joined "<module>+0x<rva>" frames (innermost first),
  // dropping the leading apifw.dll frames so the first frame is the real
  // caller. Each rva is module-relative for offline symbolization.
  // SAFETY: `frames[i]` is read only for i in [0, n); the basename scan indexes
  // `path` within the length GetModuleFileNameW returned. //base is
  // unavailable in this no-CRT DLL, so the raw indexing is scoped here.
#pragma clang unsafe_buffer_usage begin
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
        continue;  // Skip MaybeLogCallSite / the Apifw* thunk.
      }
      past_shim = true;
    }
    wchar_t path[MAX_PATH];
    const DWORD pn = mod ? ::GetModuleFileNameW(mod, path, MAX_PATH) : 0;
    const wchar_t* basename = path;
    for (DWORD c = 0; c < pn; ++c) {
      if (path[c] == L'\\' || path[c] == L'/') {
        basename = path + c + 1;
      }
    }
    if (wrote_any) {
      line.AppendChar(';');
    }
    line.AppendWide(pn ? basename : L"?");
    line.AppendChar('+');
    line.AppendHex(reinterpret_cast<uintptr_t>(frames[i]) - base);
    wrote_any = true;
  }
#pragma clang unsafe_buffer_usage end

  if (detail_narrow) {
    line.AppendChar(' ');
    line.Append(detail_narrow);
  } else if (detail_wide) {
    line.AppendChar(' ');
    line.AppendWide(detail_wide);
  }
  line.AppendChar('\n');
  Emit(line);
}

void LogFnOnceW(const char* function, const wchar_t* arg) {
  LogKey key;
  BuildDedupKey(key, function, nullptr, arg);
  if (LogOnceShouldEmit(key.CStr())) {
    LogFn(function, arg ? arg : L"");
  }
}

void LogFnOnceA(const char* function, const char* arg) {
  LogKey key;
  BuildDedupKey(key, function, arg, nullptr);
  if (LogOnceShouldEmit(key.CStr())) {
    LogFn(function, arg ? arg : "");
  }
}

void LogFnUInt(const char* function,
               const char* arg_name,
               unsigned int arg_value) {
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  line.Append(arg_name);
  line.AppendChar('=');
  line.AppendDecimal(arg_value);
  line.Append(")\n");
  Emit(line);
}

void LogHandle(const char* function, const void* handle) {
  LogLine line;
  AppendPidPrefix(line);
  line.Append(function);
  line.AppendChar('(');
  line.AppendPointer(handle);
  line.Append(")\n");
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
  line.AppendChar('\n');
  Emit(line);
}

}  // namespace basic_process
