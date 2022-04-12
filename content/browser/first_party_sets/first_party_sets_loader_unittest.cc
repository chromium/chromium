// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/first_party_sets_loader.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace content {

MATCHER_P(SerializesTo, want, "") {
  const std::string got = arg.Serialize();
  return testing::ExplainMatchResult(testing::Eq(want), got, result_listener);
}

class FirstPartySetsLoaderTest : public ::testing::Test {
 public:
  FirstPartySetsLoaderTest()
      : loader_(future_.GetCallback(), base::Value::Dict()) {}

  void SetComponentSets(FirstPartySetsLoader& sets, base::StringPiece content) {
    base::ScopedTempDir temp_dir;
    CHECK(temp_dir.CreateUniqueTempDir());
    base::FilePath path =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("sets_file.json"));
    CHECK(base::WriteFile(path, content));

    sets.SetComponentSets(
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ));
  }

  base::flat_map<net::SchemefulSite, net::SchemefulSite> WaitAndGetResult() {
    return future_.Get();
  }

  FirstPartySetsLoader& loader() { return loader_; }

 private:
  base::test::TaskEnvironment env_;
  base::test::TestFuture<base::flat_map<net::SchemefulSite, net::SchemefulSite>>
      future_;
  FirstPartySetsLoader loader_;
};

TEST_F(FirstPartySetsLoaderTest, IgnoresInvalidFile) {
  loader().SetManuallySpecifiedSet("");
  const std::string input = "certainly not valid JSON";
  SetComponentSets(loader(), input);
  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, ParsesComponent) {
  SetComponentSets(loader(), "");
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");
  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMinimal) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://aaaa.test\",],}";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://aaaa.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, AcceptsMultipleSets) {
  const std::string input =
      "{\"owner\": \"https://example.test\",\"members\": "
      "[\"https://member1.test\"]}\n"
      "{\"owner\": \"https://foo.test\",\"members\": "
      "[\"https://member2.test\"]}";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetComponentSets_Idempotent) {
  std::string input =
      R"({"owner": "https://example.test", "members": ["https://member1.test"]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";

  std::string input2 = R"({ "owner": "https://example2.test", "members":)"
                       R"( ["https://member1.test"]}
    {"owner": "https://foo2.test", "members": ["https://member2.test"]})";

  SetComponentSets(loader(), input);
  SetComponentSets(loader(), input2);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(),
              // The second call to SetComponentSets should have had no effect.
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(FirstPartySetsLoaderTest, OwnerIsOnlyMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members": ["https://example.test"]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, OwnerIsMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members":)"
      R"( ["https://example.test", "https://member1.test"]}
    {"owner": "https://foo.test", "members": ["https://member2.test"]})";
  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, RepeatedMember) {
  const std::string input =
      R"({"owner": "https://example.test", "members":)"
      R"( ["https://member1.test", "https://member2.test",)"
      R"( "https://member1.test"]}
    {"owner": "https://foo.test", "members": ["https://member3.test"]})";

  SetComponentSets(loader(), input);
  // Set required input to make sure callback gets called.
  loader().SetManuallySpecifiedSet("");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Invalid_TooSmall) {
  loader().SetManuallySpecifiedSet("https://example.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Invalid_NotOrigins) {
  loader().SetManuallySpecifiedSet("https://example.test,member1");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Invalid_NotHTTPS) {
  loader().SetManuallySpecifiedSet("https://example.test,http://member1.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Invalid_RegisteredDomain_Owner) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test..,https://www.member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Invalid_RegisteredDomain_Member) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test..");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_SingleMember) {
  loader().SetManuallySpecifiedSet("https://example.test,https://member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Valid_SingleMember_RegisteredDomain) {
  loader().SetManuallySpecifiedSet(
      "https://www.example.test,https://www.member.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_MultipleMembers) {
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_Valid_OwnerIsOnlyMember) {
  loader().SetManuallySpecifiedSet("https://example.test,https://example.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(), IsEmpty());
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_OwnerIsMember) {
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://example.test,https://member1.test");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_Valid_RepeatedMember) {
  loader().SetManuallySpecifiedSet(R"(https://example.test,
       https://member1.test,
       https://member2.test,
       https://member1.test)");
  // Set required input to make sure callback gets called.
  SetComponentSets(loader(), "");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test"))));
}

TEST_F(FirstPartySetsLoaderTest, SetsManuallySpecified_DeduplicatesOwnerOwner) {
  const std::string input = R"({"owner": "https://example.test", "members": )"
                            R"(["https://member2.test", "https://member3.test"]}
    {"owner": "https://bar.test", "members": ["https://member4.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesOwnerMember) {
  const std::string input = R"({"owner": "https://foo.test", "members": )"
                            R"(["https://member1.test", "https://example.test"]}
    {"owner": "https://bar.test", "members": ["https://member2.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member3.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://bar.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesMemberOwner) {
  const std::string input = R"({"owner": "https://foo.test", "members": )"
                            R"(["https://member1.test", "https://member2.test"]}
    {"owner": "https://member3.test", "members": ["https://member4.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet("https://example.test,https://member3.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://foo.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_DeduplicatesMemberMember) {
  const std::string input = R"({"owner": "https://foo.test", "members": )"
                            R"(["https://member2.test", "https://member3.test"]}
    {"owner": "https://bar.test", "members": ["https://member4.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet(
      "https://example.test,https://member1.test,https://member2.test");

  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member2.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://foo.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://member3.test"),
                                        SerializesTo("https://foo.test")),
                                   Pair(SerializesTo("https://bar.test"),
                                        SerializesTo("https://bar.test")),
                                   Pair(SerializesTo("https://member4.test"),
                                        SerializesTo("https://bar.test"))));
}

TEST_F(FirstPartySetsLoaderTest,
       SetsManuallySpecified_PrunesInducedSingletons) {
  const std::string input =
      R"({"owner": "https://foo.test", "members": ["https://member1.test"]})";
  SetComponentSets(loader(), input);
  loader().SetManuallySpecifiedSet("https://example.test,https://member1.test");

  // If we just erased entries that overlapped with the manually-supplied
  // set, https://foo.test would be left as a singleton set. But since we
  // disallow singleton sets, we ensure that such cases are caught and
  // removed.
  EXPECT_THAT(WaitAndGetResult(),
              UnorderedElementsAre(Pair(SerializesTo("https://example.test"),
                                        SerializesTo("https://example.test")),
                                   Pair(SerializesTo("https://member1.test"),
                                        SerializesTo("https://example.test"))));
}

}  // namespace content
