// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_UTILS_H_
#define IPC_IPC_MESSAGE_UTILS_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "base/types/id_type.h"
#include "base/values.h"
#include "build/build_config.h"
#include "ipc/ipc_buildflags.h"
#include "ipc/ipc_param_traits.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_handle.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include <lib/zx/vmo.h>
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util_win.h"
#endif

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#include "ipc/ipc_message.h"
#endif

namespace base {
class FilePath;
class Time;
class TimeDelta;
class TimeTicks;
class UnguessableToken;
struct FileDescriptor;
}  // namespace base

namespace IPC {

class Message;
struct ChannelHandle;

#if BUILDFLAG(IS_WIN)
class PlatformFileForTransit;
#endif

// -----------------------------------------------------------------------------
// How we send IPC message logs across channels.
struct COMPONENT_EXPORT(IPC) LogData {
  LogData();
  LogData(const LogData& other);
  ~LogData();

  std::string channel;
  int32_t routing_id;
  uint32_t type;  // "User-defined" message type, from ipc_message.h.
  std::string flags;
  int64_t sent;  // Time that the message was sent (i.e. at Send()).
  int64_t receive;  // Time before it was dispatched (i.e. before calling
                    // OnMessageReceived).
  int64_t dispatch;  // Time after it was dispatched (i.e. after calling
                     // OnMessageReceived).
  std::string message_name;
  std::string params;
};

//-----------------------------------------------------------------------------

// A dummy struct to place first just to allow leading commas for all
// members in the macro-generated constructor initializer lists.
struct NoParams {
};

// Specializations are checked by 'IPC checker' part of find-bad-constructs
// Clang plugin (see WriteParam() below for the details).
template <typename... Ts>
struct CheckedTuple {
  typedef std::tuple<Ts...> Tuple;
};

// This function is checked by 'IPC checker' part of find-bad-constructs
// Clang plugin to make it's not called on the following types:
// 1. long / unsigned long (but not typedefs to)
// 2. intmax_t, uintmax_t, intptr_t, uintptr_t, wint_t,
//    size_t, rsize_t, ssize_t, ptrdiff_t, dev_t, off_t, clock_t,
//    time_t, suseconds_t (including typedefs to)
// 3. Any template referencing types above (e.g. std::vector<size_t>)
template <class P>
inline void WriteParam(base::Pickle* m, const P& p) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  ParamTraits<Type>::Write(m, static_cast<const Type& >(p));
}

template <class P>
[[nodiscard]] inline bool ReadParam(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    P* p) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  return ParamTraits<Type>::Read(m, iter, reinterpret_cast<Type* >(p));
}

template <class P>
inline void LogParam(const P& p, std::string* l) {
  typedef typename SimilarTypeTraits<P>::Type Type;
  ParamTraits<Type>::Log(static_cast<const Type& >(p), l);
}

// Primitive ParamTraits -------------------------------------------------------

template <>
struct ParamTraits<bool> {
  typedef bool param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteBool(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadBool(r);
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<signed char> {
  typedef signed char param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<unsigned char> {
  typedef unsigned char param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<unsigned short> {
  typedef unsigned short param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<int> {
  typedef int param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteInt(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt(r);
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned int> {
  typedef unsigned int param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteInt(static_cast<int>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt(reinterpret_cast<int*>(r));
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

// long isn't safe to send over IPC because it's 4 bytes on 32 bit builds but
// 8 bytes on 64 bit builds. So if a 32 bit and 64 bit process have a channel
// that would cause problem.
// We need to keep this on for a few configs:
//   1) Windows because DWORD is typedef'd to it, which is fine because we have
//      very few IPCs that cross this boundary.
//   2) We also need to keep it for Linux for two reasons: int64_t is typedef'd
//      to long, and gfx::PluginWindow is long and is used in one GPU IPC.
//   3) Android 64 bit and Fuchsia also have int64_t typedef'd to long.
// Since we want to support Android 32<>64 bit IPC, as long as we don't have
// these traits for 32 bit ARM then that'll catch any errors.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) ||                                              \
    (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_64_BITS))
template <>
struct ParamTraits<long> {
  typedef long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteLong(p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadLong(r);
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned long> {
  typedef unsigned long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteLong(p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadLong(reinterpret_cast<long*>(r));
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};
#endif

template <>
struct ParamTraits<long long> {
  typedef long long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteInt64(static_cast<int64_t>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt64(reinterpret_cast<int64_t*>(r));
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<unsigned long long> {
  typedef unsigned long long param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteInt64(static_cast<int64_t>(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadInt64(reinterpret_cast<int64_t*>(r));
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

// Note that the IPC layer doesn't sanitize NaNs and +/- INF values.  Clients
// should be sure to check the sanity of these values after receiving them over
// IPC.
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<float> {
  typedef float param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteFloat(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadFloat(r);
  }
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<double> {
  typedef double param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <class P, size_t Size>
struct ParamTraits<P[Size]> {
  using param_type = P[Size];
  static void Write(base::Pickle* m, const param_type& p) {
    for (const P& element : p)
      WriteParam(m, element);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    for (P& element : *r) {
      if (!ReadParam(m, iter, &element))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("[");
    for (const P& element : p) {
      if (&element != &p[0])
        l->append(" ");
      LogParam(element, l);
    }
    l->append("]");
  }
};

// STL ParamTraits -------------------------------------------------------------

template <>
struct ParamTraits<std::string> {
  typedef std::string param_type;
  static void Write(base::Pickle* m, const param_type& p) { m->WriteString(p); }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadString(r);
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<std::u16string> {
  typedef std::u16string param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteString16(p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return iter->ReadString16(r);
  }
  COMPONENT_EXPORT(IPC) static void Log(const param_type& p, std::string* l);
};

#if BUILDFLAG(IS_WIN)
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<std::wstring> {
  typedef std::wstring param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    m->WriteString16(base::AsStringPiece16(p));
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<std::vector<char>> {
  typedef std::vector<char> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle*,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<std::vector<unsigned char>> {
  typedef std::vector<unsigned char> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<std::vector<bool>> {
  typedef std::vector<bool> param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <class P>
struct ParamTraits<std::vector<P>> {
  typedef std::vector<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::checked_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++)
      WriteParam(m, p[i]);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    size_t size;
    // ReadLength() checks for < 0 itself.
    if (!iter->ReadLength(&size))
      return false;
    // Resizing beforehand is not safe, see BUG 1006367 for details.
    if (size > INT_MAX / sizeof(P))
      return false;
    r->resize(size);
    for (size_t i = 0; i < size; i++) {
      if (!ReadParam(m, iter, &(*r)[i]))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0)
        l->append(" ");
      LogParam((p[i]), l);
    }
  }
};

template <class P>
struct ParamTraits<std::set<P> > {
  typedef std::set<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::checked_cast<int>(p.size()));
    typename param_type::const_iterator iter;
    for (iter = p.begin(); iter != p.end(); ++iter)
      WriteParam(m, *iter);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    size_t size;
    if (!iter->ReadLength(&size))
      return false;
    for (size_t i = 0; i < size; ++i) {
      P item;
      if (!ReadParam(m, iter, &item))
        return false;
      r->insert(item);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::set>");
  }
};

template <class K, class V, class C, class A>
struct ParamTraits<std::map<K, V, C, A> > {
  typedef std::map<K, V, C, A> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::checked_cast<int>(p.size()));
    for (const auto& iter : p) {
      WriteParam(m, iter.first);
      WriteParam(m, iter.second);
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0)
      return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k))
        return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::map>");
  }
};

template <class K, class V, class C, class A>
struct ParamTraits<std::unordered_map<K, V, C, A>> {
  typedef std::unordered_map<K, V, C, A> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::checked_cast<int>(p.size()));
    for (const auto& iter : p) {
      WriteParam(m, iter.first);
      WriteParam(m, iter.second);
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int size;
    if (!ReadParam(m, iter, &size) || size < 0)
      return false;
    for (int i = 0; i < size; ++i) {
      K k;
      if (!ReadParam(m, iter, &k))
        return false;
      V& value = (*r)[k];
      if (!ReadParam(m, iter, &value))
        return false;
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<std::unordered_map>");
  }
};

template <class A, class B>
struct ParamTraits<std::pair<A, B> > {
  typedef std::pair<A, B> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.first);
    WriteParam(m, p.second);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, &r->first) && ReadParam(m, iter, &r->second);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.first, l);
    l->append(", ");
    LogParam(p.second, l);
    l->append(")");
  }
};

// Base ParamTraits ------------------------------------------------------------

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::Value::Dict> {
  typedef base::Value::Dict param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// FileDescriptors may be serialised over IPC channels on POSIX. On the
// receiving side, the FileDescriptor is a valid duplicate of the file
// descriptor which was transmitted: *it is not just a copy of the integer like
// HANDLEs on Windows*. The only exception is if the file descriptor is < 0. In
// this case, the receiving end will see a value of -1. *Zero is a valid file
// descriptor*.
//
// The received file descriptor will have the |auto_close| flag set to true. The
// code which handles the message is responsible for taking ownership of it.
// File descriptors are OS resources and must be closed when no longer needed.
//
// When sending a file descriptor, the file descriptor must be valid at the time
// of transmission. Since transmission is not synchronous, one should consider
// dup()ing any file descriptors to be transmitted and setting the |auto_close|
// flag, which causes the file descriptor to be closed after writing.
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::FileDescriptor> {
  typedef base::FileDescriptor param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::ScopedFD> {
  typedef base::ScopedFD param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::win::ScopedHandle> {
  using param_type = base::win::ScopedHandle;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif

#if BUILDFLAG(IS_FUCHSIA)
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<zx::vmo> {
  typedef zx::vmo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<zx::channel> {
  typedef zx::channel param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
template <>
struct COMPONENT_EXPORT(IPC)
    ParamTraits<base::android::ScopedHardwareBufferHandle> {
  typedef base::android::ScopedHardwareBufferHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::ReadOnlySharedMemoryRegion> {
  typedef base::ReadOnlySharedMemoryRegion param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::WritableSharedMemoryRegion> {
  typedef base::WritableSharedMemoryRegion param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::UnsafeSharedMemoryRegion> {
  typedef base::UnsafeSharedMemoryRegion param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC)
    ParamTraits<base::subtle::PlatformSharedMemoryRegion> {
  typedef base::subtle::PlatformSharedMemoryRegion param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC)
    ParamTraits<base::subtle::PlatformSharedMemoryRegion::Mode> {
  typedef base::subtle::PlatformSharedMemoryRegion::Mode param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

#if BUILDFLAG(IS_WIN)
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<PlatformFileForTransit> {
  typedef PlatformFileForTransit param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // BUILDFLAG(IS_WIN)

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::FilePath> {
  typedef base::FilePath param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::Value::List> {
  typedef base::Value::List param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::Value> {
  typedef base::Value param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::File::Info> {
  typedef base::File::Info param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct SimilarTypeTraits<base::File::Error> {
  typedef int Type;
};

#if BUILDFLAG(IS_WIN)
template <>
struct SimilarTypeTraits<HWND> {
  typedef HANDLE Type;
};
#endif  // BUILDFLAG(IS_WIN)

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::Time> {
  typedef base::Time param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::TimeDelta> {
  typedef base::TimeDelta param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::TimeTicks> {
  typedef base::TimeTicks param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<base::UnguessableToken> {
  typedef base::UnguessableToken param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<std::tuple<>> {
  typedef std::tuple<> param_type;
  static void Write(base::Pickle* m, const param_type& p) {}
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
  }
};

template <typename T, int index, int count>
struct TupleParamTraitsHelper {
  using Next = TupleParamTraitsHelper<T, index + 1, count>;

  static void Write(base::Pickle* m, const T& p) {
    WriteParam(m, std::get<index>(p));
    Next::Write(m, p);
  }

  static bool Read(const base::Pickle* m, base::PickleIterator* iter, T* r) {
    return ReadParam(m, iter, &std::get<index>(*r)) && Next::Read(m, iter, r);
  }

  static void Log(const T& p, std::string* l) {
    LogParam(std::get<index>(p), l);
    if (index < count - 1)
      l->append(", ");
    Next::Log(p, l);
  }
};

template <typename T, int index>
struct TupleParamTraitsHelper<T, index, index> {
  static void Write(base::Pickle* m, const T& p) {}
  static bool Read(const base::Pickle* m, base::PickleIterator* iter, T* r) {
    return true;
  }
  static void Log(const T& p, std::string* l) {}
};

template <typename... Args>
struct ParamTraits<std::tuple<Args...>> {
  using param_type = std::tuple<Args...>;
  using Helper =
      TupleParamTraitsHelper<param_type, 0, std::tuple_size<param_type>::value>;

  static void Write(base::Pickle* m, const param_type& p) {
    Helper::Write(m, p);
  }

  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return Helper::Read(m, iter, r);
  }

  static void Log(const param_type& p, std::string* l) { Helper::Log(p, l); }
};

template <class P, size_t stack_capacity>
struct ParamTraits<absl::InlinedVector<P, stack_capacity>> {
  typedef absl::InlinedVector<P, stack_capacity> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, base::checked_cast<int>(p.size()));
    for (size_t i = 0; i < p.size(); i++) {
      WriteParam(m, p[i]);
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    size_t size;
    if (!iter->ReadLength(&size)) {
      return false;
    }
    // Sanity check for the vector size.
    if (size > INT_MAX / sizeof(P)) {
      return false;
    }
    P value;
    for (size_t i = 0; i < size; i++) {
      if (!ReadParam(m, iter, &value)) {
        return false;
      }
      r->push_back(value);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    for (size_t i = 0; i < p.size(); ++i) {
      if (i != 0) {
        l->append(" ");
      }
      LogParam((p[i]), l);
    }
  }
};

template <class Key, class Mapped, class Compare>
struct ParamTraits<base::flat_map<Key, Mapped, Compare>> {
  using param_type = base::flat_map<Key, Mapped, Compare>;
  static void Write(base::Pickle* m, const param_type& p) {
    DCHECK(base::IsValueInRangeForNumericType<int>(p.size()));
    WriteParam(m, base::checked_cast<int>(p.size()));
    for (const auto& iter : p) {
      WriteParam(m, iter.first);
      WriteParam(m, iter.second);
    }
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    size_t size;
    if (!iter->ReadLength(&size))
      return false;

    // Construct by creating in a vector and moving into the flat_map. Properly
    // serialized flat_maps will be in-order so this will be O(n). Incorrectly
    // serialized ones will still be handled properly.
    std::vector<typename param_type::value_type> vect;
    vect.resize(size);
    for (size_t i = 0; i < size; ++i) {
      if (!ReadParam(m, iter, &vect[i].first))
        return false;
      if (!ReadParam(m, iter, &vect[i].second))
        return false;
    }

    *r = param_type(std::move(vect));
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<base::flat_map>");
  }
};

template <class P>
struct ParamTraits<std::unique_ptr<P>> {
  typedef std::unique_ptr<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    bool valid = !!p;
    WriteParam(m, valid);
    if (valid)
      WriteParam(m, *p);
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    bool valid = false;
    if (!ReadParam(m, iter, &valid))
      return false;

    if (!valid) {
      r->reset();
      return true;
    }

    param_type temp(new P());
    if (!ReadParam(m, iter, temp.get()))
      return false;

    r->swap(temp);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    if (p)
      LogParam(*p, l);
    else
      l->append("NULL");
  }
};

// absl types ParamTraits

template <class P>
struct ParamTraits<std::optional<P>> {
  typedef std::optional<P> param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    const bool is_set = static_cast<bool>(p);
    WriteParam(m, is_set);
    if (is_set)
      WriteParam(m, p.value());
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    bool is_set = false;
    if (!iter->ReadBool(&is_set))
      return false;
    if (is_set) {
      P value;
      if (!ReadParam(m, iter, &value))
        return false;
      *r = std::move(value);
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    if (p)
      LogParam(p.value(), l);
    else
      l->append("(unset)");
  }
};

template <>
struct ParamTraits<absl::monostate> {
  typedef absl::monostate param_type;
  static void Write(base::Pickle* m, const param_type& p) {}
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return true;
  }
  static void Log(const param_type& p, std::string* l) { l->append("()"); }
};

// base/util types ParamTraits

template <typename TypeMarker, typename WrappedType, WrappedType kInvalidValue>
struct ParamTraits<base::IdType<TypeMarker, WrappedType, kInvalidValue>> {
  using param_type = base::IdType<TypeMarker, WrappedType, kInvalidValue>;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.GetUnsafeValue());
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    WrappedType value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = param_type::FromUnsafeValue(value);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.GetUnsafeValue(), l);
  }
};

template <typename TagType, typename UnderlyingType>
struct ParamTraits<base::StrongAlias<TagType, UnderlyingType>> {
  using param_type = base::StrongAlias<TagType, UnderlyingType>;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.value());
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    UnderlyingType value;
    if (!ReadParam(m, iter, &value))
      return false;
    *r = param_type(value);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    LogParam(p.value(), l);
  }
};

// IPC types ParamTraits -------------------------------------------------------

// A ChannelHandle is basically a platform-inspecific wrapper around the
// fact that IPC endpoints are handled specially on POSIX.  See above comments
// on FileDescriptor for more background.
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<IPC::ChannelHandle> {
  typedef ChannelHandle param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<LogData> {
  typedef LogData param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<Message> {
  static void Write(base::Pickle* m, const Message& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   Message* r);
  static void Log(const Message& p, std::string* l);
};

// Windows ParamTraits ---------------------------------------------------------

#if BUILDFLAG(IS_WIN)
template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<HANDLE> {
  typedef HANDLE param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct COMPONENT_EXPORT(IPC) ParamTraits<MSG> {
  typedef MSG param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};
#endif  // BUILDFLAG(IS_WIN)

//-----------------------------------------------------------------------------
// Generic message subclasses

// defined in ipc_logging.cc
COMPONENT_EXPORT(IPC)
void GenerateLogData(const Message& message, LogData* data, bool get_params);

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
inline void AddOutputParamsToLog(const Message* msg, std::string* l) {
  const std::string& output_params = msg->output_params();
  if (!l->empty() && !output_params.empty())
    l->append(", ");

  l->append(output_params);
}

template <class ReplyParamType>
inline void LogReplyParamsToMessage(const ReplyParamType& reply_params,
                                    const Message* msg) {
  if (msg->received_time() != 0) {
    std::string output_params;
    LogParam(reply_params, &output_params);
    msg->set_output_params(output_params);
  }
}

inline void ConnectMessageAndReply(const Message* msg, Message* reply) {
  if (msg->sent_time()) {
    // Don't log the sync message after dispatch, as we don't have the
    // output parameters at that point.  Instead, save its data and log it
    // with the outgoing reply message when it's sent.
    LogData* data = new LogData;
    GenerateLogData(*msg, data, true);
    msg->set_dont_log();
    reply->set_sync_log_data(data);
  }
}
#else
inline void AddOutputParamsToLog(const Message* msg, std::string* l) {}

template <class ReplyParamType>
inline void LogReplyParamsToMessage(const ReplyParamType& reply_params,
                                    const Message* msg) {}

inline void ConnectMessageAndReply(const Message* msg, Message* reply) {}
#endif

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_UTILS_H_
