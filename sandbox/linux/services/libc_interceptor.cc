// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/services/libc_interceptor.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "base/sanitizer_buildflags.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

#if BUILDFLAG(USING_SANITIZER) && !defined(COMPONENT_BUILD)
// Sanitizers may override certain libc functions with a weak symbol that points
// the real symbol to an interceptor symbol. E.g. getaddrinfo ->
// __interceptor_getaddrinfo. However our own libc overrides below prevent the
// weak symbol from binding, which leads to errors (especially msan errors) when
// the sanitizer interception code doesn't run. So we want to call the
// sanitizer's __interceptor_* version of the symbol if it exists.
//
// So here, using a weak symbol we can detect whether a sanitizer has overridden
// a libc symbol with its own __interceptor_* function. If it's been
// intercepted, we can call the sanitizer's version instead of the normal
// RTLD_NEXT version.
//
// Note that in component builds this isn't necessary because our override is in
// a shared object, but the sanitizer interceptor is always in the main
// executable so it is always first in the binding order. That means the
// interceptor will call our override, and calling the interceptor again is
// infinite recursion.
//
// INTERCEPTOR_DECL declares the weak symbol for the __interceptor_* version and
// REAL(func) should return the address of the __interceptor_* version of |func|
// if it exists, otherwise it returns dlsym(RTLD_NEXT, func).
#define INTERCEPTOR_DECL(ret_type, func, ...) \
  extern "C" ret_type __interceptor_##func(__VA_ARGS__) __attribute__((weak));

#define REAL(func)                                              \
  (__interceptor_##func)                                        \
      ? reinterpret_cast<decltype(&func)>(__interceptor_##func) \
      : reinterpret_cast<decltype(&func)>(dlsym(RTLD_NEXT, #func))

#else  // BUILDFLAG(USING_SANITIZER)

#define INTERCEPTOR_DECL(...)
#define REAL(func) reinterpret_cast<decltype(&func)>(dlsym(RTLD_NEXT, #func))

#endif  // BUILDFLAG(USING_SANITIZER)

// When Chrome's interceptors have overridden a libc function but need to call
// the actual libc version, the following macros take care of calling
// dlsym(RTLD_NEXT, func), storing the result, handling failures, and disabling
// CFI checks when calling the resulting pointer. See below for examples.
#define DLSYM_FUNC_DECL(ret_type, func, ...)    \
  INTERCEPTOR_DECL(ret_type, func, __VA_ARGS__) \
                                                \
  DISABLE_CFI_DLSYM                             \
  ret_type call_real_##func(__VA_ARGS__)

#define DLSYM_FUNC_BODY(func, dlsym_failed_return_val, ...) \
  static decltype(&func) fn_ptr = REAL(func);               \
                                                            \
  if (!fn_ptr) {                                            \
    LOG(ERROR) << "Cannot find " #func " with dlsym.";      \
    return dlsym_failed_return_val;                         \
  }                                                         \
                                                            \
  return fn_ptr(__VA_ARGS__);

// Used to call a |func| that's been declared with DLSYM_FUNC_DECL.
#define CALL_FUNC(func, ...) call_real_##func(__VA_ARGS__)

// A wrapper that calls libc's getaddrinfo().
DLSYM_FUNC_DECL(int,
                getaddrinfo,
                const char* node,
                const char* service,
                const struct addrinfo* hints,
                struct addrinfo** res) {
  DLSYM_FUNC_BODY(getaddrinfo, EAI_SYSTEM, node, service, hints, res)
}

namespace sandbox {

namespace {

// The global |g_am_zygote_or_renderer| is true iff we are in a zygote or
// renderer process. It's set in ZygoteMain and inherited by the renderers when
// they fork. (This means that it'll be incorrect for global constructor
// functions and before ZygoteMain is called - beware).
bool g_am_zygote_or_renderer = false;
int g_backchannel_fd = -1;

base::LazyInstance<std::set<std::string>>::Leaky g_timezones =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::Lock>::Leaky g_timezones_lock =
    LAZY_INSTANCE_INITIALIZER;

bool ReadTimeStruct(base::PickleIterator* iter,
                    struct tm* output,
                    char* timezone_out,
                    size_t timezone_out_len) {
  int result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_sec = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_min = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_hour = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_mday = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_mon = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_year = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_wday = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_yday = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_isdst = result;
  if (!iter->ReadInt(&result))
    return false;
  output->tm_gmtoff = result;

  std::string timezone;
  if (!iter->ReadString(&timezone))
    return false;
  if (timezone_out_len) {
    const size_t copy_len = std::min(timezone_out_len - 1, timezone.size());
    memcpy(timezone_out, timezone.data(), copy_len);
    timezone_out[copy_len] = 0;
    output->tm_zone = timezone_out;
  } else {
    base::AutoLock lock(g_timezones_lock.Get());
    auto ret_pair = g_timezones.Get().insert(timezone);
    output->tm_zone = ret_pair.first->c_str();
  }

  return true;
}

void WriteTimeStruct(base::Pickle* pickle, const struct tm& time) {
  pickle->WriteInt(time.tm_sec);
  pickle->WriteInt(time.tm_min);
  pickle->WriteInt(time.tm_hour);
  pickle->WriteInt(time.tm_mday);
  pickle->WriteInt(time.tm_mon);
  pickle->WriteInt(time.tm_year);
  pickle->WriteInt(time.tm_wday);
  pickle->WriteInt(time.tm_yday);
  pickle->WriteInt(time.tm_isdst);
  pickle->WriteInt(time.tm_gmtoff);
  pickle->WriteString(time.tm_zone);
}

// See
// https://chromium.googlesource.com/chromium/src/+/main/docs/linux/zygote.md
void ProxyLocaltimeCallToBrowser(time_t input,
                                 struct tm* output,
                                 char* timezone_out,
                                 size_t timezone_out_len) {
  base::Pickle request;
  request.WriteInt(METHOD_LOCALTIME);
  request.WriteString(
      std::string(reinterpret_cast<char*>(&input), sizeof(input)));

  memset(output, 0, sizeof(struct tm));

  uint8_t reply_buf[512];
  const ssize_t r = base::UnixDomainSocket::SendRecvMsg(
      g_backchannel_fd, reply_buf, sizeof(reply_buf), nullptr, request);
  if (r == -1)
    return;

  base::Pickle reply = base::Pickle::WithUnownedBuffer(
      base::span(reply_buf, base::checked_cast<size_t>(r)));
  base::PickleIterator iter(reply);
  if (!ReadTimeStruct(&iter, output, timezone_out, timezone_out_len)) {
    memset(output, 0, sizeof(struct tm));
  }
}

// The other side of this call is ProxyLocaltimeCallToBrowser().
bool HandleLocalTime(int fd,
                     base::PickleIterator iter,
                     const std::vector<base::ScopedFD>& fds) {
  std::string time_string;
  if (!iter.ReadString(&time_string) || time_string.size() != sizeof(time_t))
    return false;

  time_t time;
  memcpy(&time, time_string.data(), sizeof(time));
  struct tm expanded_time = {};
  localtime_r(&time, &expanded_time);

  base::Pickle reply;
  WriteTimeStruct(&reply, expanded_time);

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));

  struct iovec iov = {const_cast<uint8_t*>(reply.data()), reply.size()};
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  if (HANDLE_EINTR(sendmsg(fds[0].get(), &msg, MSG_DONTWAIT)) < 0)
    PLOG(ERROR) << "sendmsg";

  return true;
}

}  // namespace

typedef struct tm* (*LocaltimeFunction)(const time_t* timep);
typedef struct tm* (*LocaltimeRFunction)(const time_t* timep,
                                         struct tm* result);

static pthread_once_t g_libc_localtime_funcs_guard = PTHREAD_ONCE_INIT;
static LocaltimeFunction g_libc_localtime;
static LocaltimeFunction g_libc_localtime64;
static LocaltimeRFunction g_libc_localtime_r;
static LocaltimeRFunction g_libc_localtime64_r;

static void InitLibcLocaltimeFunctionsImpl() {
  g_libc_localtime =
      reinterpret_cast<LocaltimeFunction>(dlsym(RTLD_NEXT, "localtime"));
  g_libc_localtime64 =
      reinterpret_cast<LocaltimeFunction>(dlsym(RTLD_NEXT, "localtime64"));
  g_libc_localtime_r =
      reinterpret_cast<LocaltimeRFunction>(dlsym(RTLD_NEXT, "localtime_r"));
  g_libc_localtime64_r =
      reinterpret_cast<LocaltimeRFunction>(dlsym(RTLD_NEXT, "localtime64_r"));

  if (!g_libc_localtime || !g_libc_localtime_r) {
    // https://bugs.chromium.org/p/chromium/issues/detail?id=16800
    //
    // Nvidia's libGL.so overrides dlsym for an unknown reason and replaces
    // it with a version which doesn't work. In this case we'll get a NULL
    // result. There's not a lot we can do at this point, so we just bodge it!
    LOG(ERROR) << "Your system is broken: dlsym doesn't work! This has been "
                  "reported to be caused by Nvidia's libGL. You should expect"
                  " time related functions to misbehave. "
                  "https://bugs.chromium.org/p/chromium/issues/detail?id=16800";
  }

  if (!g_libc_localtime)
    g_libc_localtime = gmtime;
  if (!g_libc_localtime64)
    g_libc_localtime64 = g_libc_localtime;
  if (!g_libc_localtime_r)
    g_libc_localtime_r = gmtime_r;
  if (!g_libc_localtime64_r)
    g_libc_localtime64_r = g_libc_localtime_r;
}

// Define localtime_override() function with asm name "localtime", so that all
// references to localtime() will resolve to this function. Notice that we need
// to set visibility attribute to "default" to export the symbol, as it is set
// to "hidden" by default in chrome per build/common.gypi.
__attribute__((__visibility__("default"))) struct tm* localtime_override(
    const time_t* timep) __asm__("localtime");

NO_SANITIZE("cfi-icall")
__attribute__((__visibility__("default"))) struct tm* localtime_override(
    const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  }

  InitLibcLocaltimeFunctions();
  struct tm* res = g_libc_localtime(timep);
#if defined(MEMORY_SANITIZER)
  if (res)
    __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone)
    __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

// Use same trick to override localtime64(), localtime_r() and localtime64_r().
__attribute__((__visibility__("default"))) struct tm* localtime64_override(
    const time_t* timep) __asm__("localtime64");

NO_SANITIZE("cfi-icall")
__attribute__((__visibility__("default"))) struct tm* localtime64_override(
    const time_t* timep) {
  if (g_am_zygote_or_renderer) {
    static struct tm time_struct;
    static char timezone_string[64];
    ProxyLocaltimeCallToBrowser(*timep, &time_struct, timezone_string,
                                sizeof(timezone_string));
    return &time_struct;
  }

  InitLibcLocaltimeFunctions();
  struct tm* res = g_libc_localtime64(timep);
#if defined(MEMORY_SANITIZER)
  if (res)
    __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone)
    __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

__attribute__((__visibility__("default"))) struct tm* localtime_r_override(
    const time_t* timep,
    struct tm* result) __asm__("localtime_r");

NO_SANITIZE("cfi-icall")
__attribute__((__visibility__("default"))) struct tm* localtime_r_override(
    const time_t* timep,
    struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, nullptr, 0);
    return result;
  }

  InitLibcLocaltimeFunctions();
  struct tm* res = g_libc_localtime_r(timep, result);
#if defined(MEMORY_SANITIZER)
  if (res)
    __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone)
    __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

__attribute__((__visibility__("default"))) struct tm* localtime64_r_override(
    const time_t* timep,
    struct tm* result) __asm__("localtime64_r");

NO_SANITIZE("cfi-icall")
__attribute__((__visibility__("default"))) struct tm* localtime64_r_override(
    const time_t* timep,
    struct tm* result) {
  if (g_am_zygote_or_renderer) {
    ProxyLocaltimeCallToBrowser(*timep, result, nullptr, 0);
    return result;
  }

  InitLibcLocaltimeFunctions();
  struct tm* res = g_libc_localtime64_r(timep, result);
#if defined(MEMORY_SANITIZER)
  if (res)
    __msan_unpoison(res, sizeof(*res));
  if (res->tm_zone)
    __msan_unpoison_string(res->tm_zone);
#endif
  return res;
}

void SetAmZygoteOrRenderer(bool enable, int backchannel_fd) {
  g_am_zygote_or_renderer = enable;
  g_backchannel_fd = backchannel_fd;
}

bool HandleInterceptedCall(int kind,
                           int fd,
                           base::PickleIterator iter,
                           const std::vector<base::ScopedFD>& fds) {
  if (kind != METHOD_LOCALTIME)
    return false;

  return HandleLocalTime(fd, iter, fds);
}

void InitLibcLocaltimeFunctions() {
  CHECK_EQ(0, pthread_once(&g_libc_localtime_funcs_guard,
                           InitLibcLocaltimeFunctionsImpl));
}

namespace {
std::atomic<bool> g_getaddrinfo_discouraged{false};
}  // namespace

extern "C" {
__attribute__((visibility("default"), noinline)) int getaddrinfo(
    const char* node,
    const char* service,
    const struct addrinfo* hints,
    struct addrinfo** res) {
  if (g_getaddrinfo_discouraged.load(std::memory_order_relaxed)) {
    DLOG(FATAL) << "Called getaddrinfo() in a sandboxed process.";
    base::debug::DumpWithoutCrashing();
    // In non-debug builds, deliberately fall through to call the real version.
  }

  return CALL_FUNC(getaddrinfo, node, service, hints, res);
}
}

void DiscourageGetaddrinfo() {
  g_getaddrinfo_discouraged = true;
}

}  // namespace sandbox
