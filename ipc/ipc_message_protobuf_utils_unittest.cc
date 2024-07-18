// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "build/build_config.h"

#include "ipc/ipc_message_protobuf_utils.h"

#include <initializer_list>

#include "ipc/test_proto.pb.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {

template <>
struct ParamTraits<ipc_message_utils_test::TestMessage1> {
  typedef ipc_message_utils_test::TestMessage1 param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.number());
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    int number;
    if (!iter->ReadInt(&number))
      return false;
    r->set_number(number);
    return true;
  }
};

template <>
struct ParamTraits<ipc_message_utils_test::TestMessage2> {
  typedef ipc_message_utils_test::TestMessage2 param_type;
  static void Write(base::Pickle* m, const param_type& p) {
    WriteParam(m, p.numbers());
    WriteParam(m, p.strings());
    WriteParam(m, p.messages());
  }
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r) {
    return ReadParam(m, iter, r->mutable_numbers()) &&
           ReadParam(m, iter, r->mutable_strings()) &&
           ReadParam(m, iter, r->mutable_messages());
  }
};

namespace {

template <class P1, class P2>
void AssertEqual(const P1& left, const P2& right) {
  ASSERT_EQ(left, right);
}

template<>
void AssertEqual(const int& left,
                 const ipc_message_utils_test::TestMessage1& right) {
  ASSERT_EQ(left, right.number());
}

template <template<class> class RepeatedFieldLike, class P1, class P2>
void AssertRepeatedFieldEquals(std::initializer_list<P1> expected,
                               const RepeatedFieldLike<P2>& fields) {
  ASSERT_EQ(static_cast<int>(expected.size()), fields.size());
  auto it = expected.begin();
  int i = 0;
  for (; it != expected.end(); it++, i++) {
    AssertEqual(*it, fields.Get(i));
  }
}

TEST(IPCMessageRepeatedFieldUtilsTest, RepeatedFieldShouldBeSerialized) {
  ipc_message_utils_test::TestMessage2 message;
  message.add_numbers(1);
  message.add_numbers(100);
  message.add_strings("abc");
  message.add_strings("def");
  message.add_messages()->set_number(1000);
  message.add_messages()->set_number(10000);

  base::Pickle pickle;
  IPC::WriteParam(&pickle, message);

  base::PickleIterator iter(pickle);
  ipc_message_utils_test::TestMessage2 output;
  ASSERT_TRUE(IPC::ReadParam(&pickle, &iter, &output));

  AssertRepeatedFieldEquals({1, 100}, output.numbers());
  AssertRepeatedFieldEquals({"abc", "def"}, output.strings());
  AssertRepeatedFieldEquals({1000, 10000}, output.messages());
}

TEST(IPCMessageRepeatedFieldUtilsTest,
     PartialEmptyRepeatedFieldShouldBeSerialized) {
  ipc_message_utils_test::TestMessage2 message;
  message.add_numbers(1);
  message.add_numbers(100);
  message.add_messages()->set_number(1000);
  message.add_messages()->set_number(10000);

  base::Pickle pickle;
  IPC::WriteParam(&pickle, message);

  base::PickleIterator iter(pickle);
  ipc_message_utils_test::TestMessage2 output;
  ASSERT_TRUE(IPC::ReadParam(&pickle, &iter, &output));

  AssertRepeatedFieldEquals({1, 100}, output.numbers());
  ASSERT_EQ(0, output.strings_size());
  AssertRepeatedFieldEquals({1000, 10000}, output.messages());
}

TEST(IPCMessageRepeatedFieldUtilsTest, EmptyRepeatedFieldShouldBeSerialized) {
  ipc_message_utils_test::TestMessage2 message;

  base::Pickle pickle;
  IPC::WriteParam(&pickle, message);

  base::PickleIterator iter(pickle);
  ipc_message_utils_test::TestMessage2 output;
  ASSERT_TRUE(IPC::ReadParam(&pickle, &iter, &output));

  ASSERT_EQ(0, output.numbers_size());
  ASSERT_EQ(0, output.strings_size());
  ASSERT_EQ(0, output.messages_size());
}

TEST(IPCMessageRepeatedFieldUtilsTest,
     InvalidPickleShouldNotCrashRepeatedFieldDeserialization) {
  base::Pickle pickle;
  IPC::WriteParam(&pickle, INT_MAX);
  IPC::WriteParam(&pickle, 0);
  IPC::WriteParam(&pickle, INT_MAX);
  IPC::WriteParam(&pickle, std::string());
  IPC::WriteParam(&pickle, 0);

  base::PickleIterator iter(pickle);
  ipc_message_utils_test::TestMessage2 output;
  ASSERT_FALSE(IPC::ReadParam(&pickle, &iter, &output));
}

// This test needs ~20 seconds in Debug mode, or ~4 seconds in Release mode.
// See http://crbug.com/741866 for details.
TEST(IPCMessageRepeatedFieldUtilsTest,
     DISABLED_InvalidPickleShouldNotCrashRepeatedFieldDeserialization2) {
  base::Pickle pickle;
  IPC::WriteParam(&pickle, 256 * 1024 * 1024);
  IPC::WriteParam(&pickle, 0);
  IPC::WriteParam(&pickle, INT_MAX);
  IPC::WriteParam(&pickle, std::string());
  IPC::WriteParam(&pickle, 0);

  base::PickleIterator iter(pickle);
  ipc_message_utils_test::TestMessage2 output;
  ASSERT_FALSE(IPC::ReadParam(&pickle, &iter, &output));
}

}  // namespace

}  // namespace IPC
