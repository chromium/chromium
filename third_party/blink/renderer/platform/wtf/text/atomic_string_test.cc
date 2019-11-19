/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(AtomicStringTest, Number) {
  int16_t int_value = 1234;
  EXPECT_EQ("1234", AtomicString::Number(int_value));
  int_value = -1234;
  EXPECT_EQ("-1234", AtomicString::Number(int_value));
  uint16_t unsigned_value = 1234u;
  EXPECT_EQ("1234", AtomicString::Number(unsigned_value));
  int32_t long_value = 6553500;
  EXPECT_EQ("6553500", AtomicString::Number(long_value));
  long_value = -6553500;
  EXPECT_EQ("-6553500", AtomicString::Number(long_value));
  uint32_t unsigned_long_value = 4294967295u;
  EXPECT_EQ("4294967295", AtomicString::Number(unsigned_long_value));
  int64_t longlong_value = 9223372036854775807;
  EXPECT_EQ("9223372036854775807", AtomicString::Number(longlong_value));
  longlong_value = -9223372036854775807;
  EXPECT_EQ("-9223372036854775807", AtomicString::Number(longlong_value));
  uint64_t unsigned_long_long_value = 18446744073709551615u;
  EXPECT_EQ("18446744073709551615",
            AtomicString::Number(unsigned_long_long_value));
  double double_value = 1234.56;
  EXPECT_EQ("1234.56", AtomicString::Number(double_value));
  double_value = 1234.56789;
  EXPECT_EQ("1234.56789", AtomicString::Number(double_value, 9));
}

TEST(AtomicStringTest, ImplEquality) {
  AtomicString foo("foo");
  AtomicString bar("bar");
  AtomicString baz("baz");
  AtomicString foo2("foo");
  AtomicString baz2("baz");
  AtomicString bar2("bar");
  EXPECT_EQ(foo.Impl(), foo2.Impl());
  EXPECT_EQ(bar.Impl(), bar2.Impl());
  EXPECT_EQ(baz.Impl(), baz2.Impl());
  EXPECT_NE(foo.Impl(), bar.Impl());
  EXPECT_NE(foo.Impl(), baz.Impl());
  EXPECT_NE(bar.Impl(), baz.Impl());
}

}  // namespace WTF
