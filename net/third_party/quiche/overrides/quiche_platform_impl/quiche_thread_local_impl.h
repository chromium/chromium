// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_THREAD_LOCAL_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_THREAD_LOCAL_IMPL_H_

#include "base/no_destructor.h"
#include "base/threading/thread_local.h"

#define DEFINE_QUICHE_THREAD_LOCAL_POINTER_IMPL(name, type)                   \
  struct QuicheThreadLocalPointer_##name {                                    \
    static ::base::ThreadLocalPointer<type>* Instance() {                     \
      static ::base::NoDestructor<::base::ThreadLocalPointer<type>> instance; \
      return instance.get();                                                  \
    }                                                                         \
    static type* Get() { return Instance()->Get(); }                          \
    static void Set(type* ptr) { Instance()->Set(ptr); }                      \
  }

#define GET_QUICHE_THREAD_LOCAL_POINTER_IMPL(name) \
  QuicheThreadLocalPointer_##name::Get()

#define SET_QUICHE_THREAD_LOCAL_POINTER_IMPL(name, value) \
  QuicheThreadLocalPointer_##name::Set(value)

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_THREAD_LOCAL_IMPL_H_
