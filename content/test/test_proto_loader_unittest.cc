// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_proto_loader.h"

#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "content/test/test.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

base::FilePath GetTestDataRoot() {
  base::FilePath test_data_root =
      base::PathService::CheckedGet(base::DIR_GEN_TEST_DATA_ROOT);
#if !BUILDFLAG(IS_FUCHSIA)
  test_data_root = test_data_root.Append(FILE_PATH_LITERAL("gen"));
#endif  // !BUILDFLAG(IS_FUCHSIA)
  return test_data_root;
}

void LoadTestProto(const std::string& proto_text,
                   google::protobuf::MessageLite& proto) {
  content::TestProtoLoader loader;
  std::string serialized_message;
  loader.ParseFromText(
      GetTestDataRoot().Append(
          FILE_PATH_LITERAL("content/test/test_proto.descriptor")),
      "content.test.TestMessage", proto_text, serialized_message);
  ASSERT_TRUE(proto.ParseFromString(serialized_message));
}

TEST(TestProtoLoaderTest, LoadProto) {
  test::TestMessage proto;
  LoadTestProto(
      R"pb(
        test: "Message"
      )pb",
      proto);
  EXPECT_EQ("Message", proto.test());
}

}  // namespace
}  // namespace content
