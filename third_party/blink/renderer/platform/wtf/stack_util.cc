// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "third_party/blink/renderer/platform/wtf/stack_util.h"

#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

#if defined(OS_WIN)
#include <stddef.h>
#include <windows.h>
#include <winnt.h>
#elif defined(__GLIBC__)
extern "C" void* __libc_stack_end;  // NOLINT
#endif

namespace WTF {

size_t GetUnderestimatedStackSize() {
// FIXME: ASAN bot uses a fake stack as a thread stack frame,
// and its size is different from the value which APIs tells us.
#if defined(ADDRESS_SANITIZER)
  return 0;
#endif

// FIXME: On Mac OSX and Linux, this method cannot estimate stack size
// correctly for the main thread.

#if defined(__GLIBC__) || defined(OS_ANDROID) || defined(OS_FREEBSD) || \
    defined(OS_FUCHSIA)
  // pthread_getattr_np() can fail if the thread is not invoked by
  // pthread_create() (e.g., the main thread of blink_unittests).
  // If so, a conservative size estimate is returned.

  pthread_attr_t attr;
  int error;
#if defined(OS_FREEBSD)
  pthread_attr_init(&attr);
  error = pthread_attr_get_np(pthread_self(), &attr);
#else
  error = pthread_getattr_np(pthread_self(), &attr);
#endif
  if (!error) {
    void* base;
    size_t size;
    error = pthread_attr_getstack(&attr, &base, &size);
    CHECK(!error);
    pthread_attr_destroy(&attr);
    return size;
  }
#if defined(OS_FREEBSD)
  pthread_attr_destroy(&attr);
#endif

  // Return a 512k stack size, (conservatively) assuming the following:
  //  - that size is much lower than the pthreads default (x86 pthreads has a 2M
  //    default.)
  //  - no one is running Blink with an RLIMIT_STACK override, let alone as
  //    low as 512k.
  //
  return 512 * 1024;
#elif defined(OS_MACOSX)
  // pthread_get_stacksize_np() returns too low a value for the main thread on
  // OSX 10.9,
  // http://mail.openjdk.java.net/pipermail/hotspot-dev/2013-October/011369.html
  //
  // Multiple workarounds possible, adopt the one made by
  // https://github.com/robovm/robovm/issues/274
  // (cf.
  // https://developer.apple.com/library/mac/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/CreatingThreads.html
  // on why hardcoding sizes is reasonable.)
  if (pthread_main_np()) {
#if defined(IOS)
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    size_t guardSize = 0;
    pthread_attr_getguardsize(&attr, &guardSize);
    // Stack size for the main thread is 1MB on iOS including the guard page
    // size.
    return (1 * 1024 * 1024 - guardSize);
#else
    // Stack size for the main thread is 8MB on OSX excluding the guard page
    // size.
    return (8 * 1024 * 1024);
#endif
  }
  return pthread_get_stacksize_np(pthread_self());
#elif defined(OS_WIN) && defined(COMPILER_MSVC)
return Threading::ThreadStackSize();
#else
#error "Stack frame size estimation not supported on this platform."
  return 0;
#endif
}

void* GetStackStart() {
#if defined(__GLIBC__) || defined(OS_ANDROID) || defined(OS_FREEBSD) || \
    defined(OS_FUCHSIA)
  pthread_attr_t attr;
  int error;
#if defined(OS_FREEBSD)
  pthread_attr_init(&attr);
  error = pthread_attr_get_np(pthread_self(), &attr);
#else
  error = pthread_getattr_np(pthread_self(), &attr);
#endif
  if (!error) {
    void* base;
    size_t size;
    error = pthread_attr_getstack(&attr, &base, &size);
    CHECK(!error);
    pthread_attr_destroy(&attr);
    return reinterpret_cast<uint8_t*>(base) + size;
  }
#if defined(OS_FREEBSD)
  pthread_attr_destroy(&attr);
#endif
#if defined(__GLIBC__)
  // pthread_getattr_np can fail for the main thread. In this case
  // just like NaCl we rely on the __libc_stack_end to give us
  // the start of the stack.
  // See https://code.google.com/p/nativeclient/issues/detail?id=3431.
  return __libc_stack_end;
#else
  NOTREACHED();
  return nullptr;
#endif
#elif defined(OS_MACOSX)
  return pthread_get_stackaddr_np(pthread_self());
#elif defined(OS_WIN) && defined(COMPILER_MSVC)
// On Windows stack limits for the current thread are available in
// the thread information block (TIB). Its fields can be accessed through
// FS segment register on x86 and GS segment register on x86_64.
// On Windows ARM64, stack limits could be retrieved by calling
// GetCurrentThreadStackLimits. This API doesn't work on x86 and x86_64 here
// because it requires Windows 8+.
#if defined(ARCH_CPU_X86_64)
  return reinterpret_cast<void*>(__readgsqword(offsetof(NT_TIB64, StackBase)));
#elif defined(ARCH_CPU_X86)
  return reinterpret_cast<void*>(__readfsdword(offsetof(NT_TIB, StackBase)));
#elif defined(ARCH_CPU_ARM64)
  ULONG_PTR lowLimit, highLimit;
  ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
  return reinterpret_cast<void*>(highLimit);
#endif
#else
#error Unsupported getStackStart on this platform.
#endif
}

uintptr_t GetCurrentStackPosition() {
#if defined(COMPILER_MSVC)
  return reinterpret_cast<uintptr_t>(_AddressOfReturnAddress());
#else
  return reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
#endif
}

namespace internal {

uintptr_t g_main_thread_stack_start = 0;
uintptr_t g_main_thread_underestimated_stack_size = 0;

void InitializeMainThreadStackEstimate() {
  // getStackStart is exclusive, not inclusive (i.e. it points past the last
  // page of the stack in linear order). So, to ensure an inclusive comparison,
  // subtract here and below.
  g_main_thread_stack_start =
      reinterpret_cast<uintptr_t>(GetStackStart()) - sizeof(void*);

  size_t underestimated_stack_size = GetUnderestimatedStackSize();
  if (underestimated_stack_size > sizeof(void*)) {
    underestimated_stack_size = underestimated_stack_size - sizeof(void*);
  }
  g_main_thread_underestimated_stack_size = underestimated_stack_size;
}

#if defined(OS_WIN) && defined(COMPILER_MSVC)
size_t ThreadStackSize() {
  // Notice that we cannot use the TIB's StackLimit for the stack end, as i
  // tracks the end of the committed range. We're after the end of the reserved
  // stack area (most of which will be uncommitted, most times.)
  MEMORY_BASIC_INFORMATION stack_info;
  memset(&stack_info, 0, sizeof(MEMORY_BASIC_INFORMATION));
  size_t result_size =
      VirtualQuery(&stack_info, &stack_info, sizeof(MEMORY_BASIC_INFORMATION));
  DCHECK_GE(result_size, sizeof(MEMORY_BASIC_INFORMATION));
  uint8_t* stack_end = reinterpret_cast<uint8_t*>(stack_info.AllocationBase);

  uint8_t* stack_start = reinterpret_cast<uint8_t*>(WTF::GetStackStart());
  CHECK(stack_start);
  CHECK_GT(stack_start, stack_end);
  size_t thread_stack_size = static_cast<size_t>(stack_start - stack_end);
  // When the third last page of the reserved stack is accessed as a
  // guard page, the second last page will be committed (along with removing
  // the guard bit on the third last) _and_ a stack overflow exception
  // is raised.
  //
  // We have zero interest in running into stack overflow exceptions while
  // marking objects, so simply consider the last three pages + one above
  // as off-limits and adjust the reported stack size accordingly.
  //
  // http://blogs.msdn.com/b/satyem/archive/2012/08/13/thread-s-stack-memory-management.aspx
  // explains the details.
  CHECK_GT(thread_stack_size, 4u * 0x1000);
  thread_stack_size -= 4 * 0x1000;
  return thread_stack_size;
}
#endif

}  // namespace internal

}  // namespace WTF
