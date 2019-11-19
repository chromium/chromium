// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/libc_interceptor.h"

#include <dlfcn.h>
#include <fcntl.h>
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
#include "base/lazy_instance.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "base/synchronization/lock.h"

namespace sandbox {

namespace {

// The global |g_am_zygote_or_renderer| is true iff we are in a zygote or
// renderer process. It's set in ZygoteMain and inherited by the renderers when
// they fork. (This means that it'll be incorrect for global constructor
// functions and before ZygoteMain is called - beware).
bool g_am_zygote_or_renderer = false;
bool g_use_localtime_override = true;
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
// https://chromium.googlesource.com/chromium/src/+/master/docs/linux_zygote.md
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

  base::Pickle reply(reinterpret_cast<char*>(reply_buf), r);
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

  struct iovec iov = {const_cast<void*>(reply.data()), reply.size()};
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
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
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
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
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
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
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
  if (g_am_zygote_or_renderer && g_use_localtime_override) {
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

void SetUseLocaltimeOverride(bool enable) {
  g_use_localtime_override = enable;
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

}  // namespace sandbox
