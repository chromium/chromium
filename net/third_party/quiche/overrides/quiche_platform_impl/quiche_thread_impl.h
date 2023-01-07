// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_THREAD_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_THREAD_IMPL_H_

#include "base/threading/simple_thread.h"

namespace quiche {

// A class representing a thread of execution in QUIC.
class QuicheThreadImpl : public base::SimpleThread {
 public:
  explicit QuicheThreadImpl(const std::string& string)
      : base::SimpleThread(string) {}
  QuicheThreadImpl(const QuicheThreadImpl&) = delete;
  QuicheThreadImpl& operator=(const QuicheThreadImpl&) = delete;
};

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_THREAD_IMPL_H_
