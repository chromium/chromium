// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_socket_pool.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/rand_callback.h"
#include "net/socket/client_socket_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class DummyObject {
 public:
  DummyObject() {}

  base::WeakPtr<DummyObject> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  bool HasWeakPtrs() const { return weak_factory_.HasWeakPtrs(); }

 private:
  base::WeakPtrFactory<DummyObject> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DummyObject);
};

class DummyRandIntCallback {
 public:
  DummyRandIntCallback() = default;

  RandIntCallback MakeCallback() {
    return base::Bind(&DummyRandIntCallback::GetRandInt, dummy_.GetWeakPtr());
  }

  bool HasRefs() const { return dummy_.HasWeakPtrs(); }

 private:
  static int GetRandInt(base::WeakPtr<DummyObject> dummy, int from, int to) {
    // Chosen by fair dice roll. Guaranteed to be random.
    return 4;
  }

  DummyObject dummy_;

  DISALLOW_COPY_AND_ASSIGN(DummyRandIntCallback);
};

// Since the below tests rely upon it, make sure that DummyRandIntCallback
// can reliably tell whether there are other refs to the callback it returns.

// A const reference to the callback shouldn't keep the callback referenced.
TEST(DummyRandIntCallbackTest, Referenced) {
  DummyRandIntCallback dummy;

  RandIntCallback original = dummy.MakeCallback();
  EXPECT_TRUE(dummy.HasRefs());
  const RandIntCallback& reference = original;
  EXPECT_TRUE(dummy.HasRefs());

  EXPECT_EQ(4, reference.Run(0, 6));

  original.Reset();
  EXPECT_FALSE(dummy.HasRefs());
}

// A copy of the callback should keep the callback referenced.
TEST(DummyRandIntCallbackTest, Copied) {
  DummyRandIntCallback dummy;

  RandIntCallback original = dummy.MakeCallback();
  EXPECT_TRUE(dummy.HasRefs());
  RandIntCallback copy = original;
  EXPECT_TRUE(dummy.HasRefs());

  EXPECT_EQ(4, copy.Run(0, 6));

  original.Reset();
  EXPECT_TRUE(dummy.HasRefs());
}

class DnsSocketPoolTest : public ::testing::Test {
 protected:
  DummyRandIntCallback dummy_;
  std::unique_ptr<DnsSocketPool> pool_;
};

// Make sure that the DnsSocketPools returned by CreateDefault and CreateNull
// both retain (by copying the RandIntCallback object, instead of taking a
// reference) the RandIntCallback used for creating sockets.

TEST_F(DnsSocketPoolTest, DefaultCopiesCallback) {
  pool_ = DnsSocketPool::CreateDefault(ClientSocketFactory::GetDefaultFactory(),
                                       dummy_.MakeCallback());
  EXPECT_TRUE(dummy_.HasRefs());
}

TEST_F(DnsSocketPoolTest, NullCopiesCallback) {
  pool_ = DnsSocketPool::CreateNull(ClientSocketFactory::GetDefaultFactory(),
                                    dummy_.MakeCallback());
  EXPECT_TRUE(dummy_.HasRefs());
}

}  // namespace
}  // namespace net
