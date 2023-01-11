// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/nsswitch_reader.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class TestFileReader {
 public:
  explicit TestFileReader(std::string text) : text_(std::move(text)) {}
  TestFileReader(const TestFileReader&) = delete;
  TestFileReader& operator=(const TestFileReader&) = delete;

  NsswitchReader::FileReadCall GetFileReadCall() {
    return base::BindRepeating(&TestFileReader::ReadFile,
                               base::Unretained(this));
  }

  std::string ReadFile() {
    CHECK(!already_read_);

    already_read_ = true;
    return text_;
  }

 private:
  std::string text_;
  bool already_read_ = false;
};

class NsswitchReaderTest : public testing::Test {
 public:
  NsswitchReaderTest() = default;
  NsswitchReaderTest(const NsswitchReaderTest&) = delete;
  NsswitchReaderTest& operator=(const NsswitchReaderTest&) = delete;

 protected:
  NsswitchReader reader_;
};

// Attempt to load the actual nsswitch.conf for the test machine and run
// rationality checks for the result.
TEST_F(NsswitchReaderTest, ActualReadAndParseHosts) {
  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  // Assume nobody will ever run this on a machine with more than 1000
  // configured services.
  EXPECT_THAT(services, testing::SizeIs(testing::Le(1000u)));

  // Assume no service will ever have more than 10 configured actions per
  // service.
  for (const NsswitchReader::ServiceSpecification& service : services) {
    EXPECT_THAT(service.actions, testing::SizeIs(testing::Le(10u)));
  }
}

TEST_F(NsswitchReaderTest, FileReadErrorResultsInDefault) {
  TestFileReader file_reader("");
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  // Expect "files dns".
  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)));
}

TEST_F(NsswitchReaderTest, MissingHostsResultsInDefault) {
  const std::string kFile =
      "passwd: files ldap\nshadow: files\ngroup: files ldap\n";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  // Expect "files dns".
  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)));
}

TEST_F(NsswitchReaderTest, ParsesAllKnownServices) {
  const std::string kFile =
      "hosts: files dns mdns mdns4 mdns6 mdns_minimal mdns4_minimal "
      "mdns6_minimal myhostname resolve nis";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns4),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns6),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMdnsMinimal),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMdns4Minimal),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMdns6Minimal),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMyHostname),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kResolve),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kNis)));
}

TEST_F(NsswitchReaderTest, ParsesRepeatedServices) {
  const std::string kFile = "hosts: mdns4 mdns6 mdns6 myhostname";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns4),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns6),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns6),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMyHostname)));
}

TEST_F(NsswitchReaderTest, ParsesAllKnownActions) {
  const std::string kFile =
      "hosts: files [UNAVAIL=RETURN] [UNAVAIL=CONTINUE] [UNAVAIL=MERGE]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kFiles,
                  {{/*negated=*/false, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kContinue},
                   {/*negated=*/false, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kMerge}})));
}

TEST_F(NsswitchReaderTest, ParsesAllKnownStatuses) {
  const std::string kFile =
      "hosts: dns [SUCCESS=RETURN] [NOTFOUND=RETURN] [UNAVAIL=RETURN] "
      "[TRYAGAIN=RETURN]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kDns,
                  {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kNotFound,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kTryAgain,
                    NsswitchReader::Action::kReturn}})));
}

TEST_F(NsswitchReaderTest, ParsesRepeatedActions) {
  const std::string kFile =
      "hosts: nis [!SUCCESS=RETURN] [NOTFOUND=RETURN] [NOTFOUND=RETURN] "
      "[!UNAVAIL=RETURN]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kNis,
                  {{/*negated=*/true, NsswitchReader::Status::kSuccess,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kNotFound,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kNotFound,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/true, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kReturn}})));
}

TEST_F(NsswitchReaderTest, ParsesCombinedActionLists) {
  const std::string kFile =
      "hosts: dns [SUCCESS=RETURN !NOTFOUND=RETURN UNAVAIL=RETURN] files";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(
                  NsswitchReader::ServiceSpecification(
                      NsswitchReader::Service::kDns,
                      {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                        NsswitchReader::Action::kReturn},
                       {/*negated=*/true, NsswitchReader::Status::kNotFound,
                        NsswitchReader::Action::kReturn},
                       {/*negated=*/false, NsswitchReader::Status::kUnavailable,
                        NsswitchReader::Action::kReturn}}),
                  NsswitchReader::ServiceSpecification(
                      NsswitchReader::Service::kFiles)));
}

TEST_F(NsswitchReaderTest, HandlesAtypicalWhitespace) {
  const std::string kFile =
      " database:  service   \n\n   hosts: files\tdns   mdns4 \t mdns6    \t  "
      "\t\n\t\n";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kMdns4),
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMdns6)));
}

TEST_F(NsswitchReaderTest, HandlesAtypicalWhitespaceInActions) {
  const std::string kFile =
      "hosts: dns [ !UNAVAIL=MERGE \t NOTFOUND=RETURN\t][ UNAVAIL=CONTINUE]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kDns,
                  {{/*negated=*/true, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kMerge},
                   {/*negated=*/false, NsswitchReader::Status::kNotFound,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kContinue}})));
}

TEST_F(NsswitchReaderTest, ParsesActionsWithoutService) {
  const std::string kFile = "hosts: [SUCCESS=RETURN]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kUnknown,
                  {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                    NsswitchReader::Action::kReturn}})));
}

TEST_F(NsswitchReaderTest, ParsesNegatedActions) {
  const std::string kFile =
      "hosts: mdns_minimal [!UNAVAIL=RETURN] [NOTFOUND=CONTINUE] "
      "[!TRYAGAIN=CONTINUE]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kMdnsMinimal,
                  {{/*negated=*/true, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kReturn},
                   {/*negated=*/false, NsswitchReader::Status::kNotFound,
                    NsswitchReader::Action::kContinue},
                   {/*negated=*/true, NsswitchReader::Status::kTryAgain,
                    NsswitchReader::Action::kContinue}})));
}

TEST_F(NsswitchReaderTest, ParsesUnrecognizedServiceAsUnknown) {
  const std::string kFile =
      "passwd: files\nhosts: files super_awesome_service myhostname";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kFiles),
                                   NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kUnknown),
                                   NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kMyHostname)));
}

TEST_F(NsswitchReaderTest, ParsesUnrecognizedStatusAsUnknown) {
  const std::string kFile =
      "hosts: nis [HELLO=CONTINUE]\nshadow: service\ndatabase: cheese";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kNis,
                  {{/*negated=*/false, NsswitchReader::Status::kUnknown,
                    NsswitchReader::Action::kContinue}})));
}

TEST_F(NsswitchReaderTest, ParsesUnrecognizedActionAsUnknown) {
  const std::string kFile =
      "more: service\nhosts: mdns6 [!UNAVAIL=HI]\nshadow: service";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kMdns6,
                  {{/*negated=*/true, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kUnknown}})));
}

TEST_F(NsswitchReaderTest, ParsesInvalidActionsAsUnknown) {
  const std::string kFile = "hosts: mdns_minimal [a=b=c] nis";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMdnsMinimal,
              {{/*negated=*/false, NsswitchReader::Status::kUnknown,
                NsswitchReader::Action::kUnknown}}),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kNis)));
}

TEST_F(NsswitchReaderTest, IgnoresInvalidlyClosedActions) {
  const std::string kFile = "hosts: myhostname [SUCCESS=MERGE";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kMyHostname,
                  {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                    NsswitchReader::Action::kMerge}})));
}

TEST_F(NsswitchReaderTest, ParsesServicesAfterInvalidlyClosedActionsAsUnknown) {
  const std::string kFile = "hosts: resolve [SUCCESS=CONTINUE dns";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kResolve,
                  {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                    NsswitchReader::Action::kContinue},
                   {/*negated=*/false, NsswitchReader::Status::kUnknown,
                    NsswitchReader::Action::kUnknown}})));
}

TEST_F(NsswitchReaderTest, IgnoresComments) {
  const std::string kFile =
      "#hosts: files super_awesome_service myhostname\nnetmask: service";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  // Expect "files dns" due to not finding an uncommented "hosts:" row.
  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kFiles),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)));
}

TEST_F(NsswitchReaderTest, IgnoresEndOfLineComments) {
  const std::string kFile =
      "hosts: files super_awesome_service myhostname # dns";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kFiles),
                                   NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kUnknown),
                                   NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kMyHostname)));
}

TEST_F(NsswitchReaderTest, IgnoresCapitalization) {
  const std::string kFile = "HoStS: mDNS6 [!uNaVaIl=MeRgE]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kMdns6,
                  {{/*negated=*/true, NsswitchReader::Status::kUnavailable,
                    NsswitchReader::Action::kMerge}})));
}

TEST_F(NsswitchReaderTest, IgnoresEmptyActions) {
  const std::string kFile = "hosts: mdns_minimal [ \t ][] [ ]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kMdnsMinimal)));
}

TEST_F(NsswitchReaderTest, IgnoresRepeatedActionBrackets) {
  const std::string kFile = "hosts: mdns [[SUCCESS=RETURN]]]dns";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(
      services,
      testing::ElementsAre(
          NsswitchReader::ServiceSpecification(
              NsswitchReader::Service::kMdns,
              {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                NsswitchReader::Action::kReturn}}),
          NsswitchReader::ServiceSpecification(NsswitchReader::Service::kDns)));
}

TEST_F(NsswitchReaderTest, IgnoresRepeatedActionBracketsWithWhitespace) {
  const std::string kFile = "hosts: mdns [ [ SUCCESS=RETURN ]\t] ]\t  mdns6";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(
                  NsswitchReader::ServiceSpecification(
                      NsswitchReader::Service::kMdns,
                      {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                        NsswitchReader::Action::kReturn}}),
                  NsswitchReader::ServiceSpecification(
                      NsswitchReader::Service::kMdns6)));
}

TEST_F(NsswitchReaderTest, RejectsNonSensicalActionBrackets) {
  const std::string kFile = "hosts: mdns4 [UNAVAIL[=MERGE]]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kMdns4,
                  {{/*negated=*/false, NsswitchReader::Status::kUnknown,
                    NsswitchReader::Action::kMerge}})));
}

TEST_F(NsswitchReaderTest, RejectsServicesWithBrackets) {
  const std::string kFile = "hosts: se]r[vice[name";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kUnknown)));
}

// Other than the case of repeating opening brackets, nested brackets are not
// valid and should just get treated as part of an action label.
TEST_F(NsswitchReaderTest, RejectsNestedActionBrackets) {
  const std::string kFile =
      "hosts: nis [SUCCESS=RETURN [NOTFOUND=CONTINUE] UNAVAIL=MERGE]";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(
                  NsswitchReader::ServiceSpecification(
                      NsswitchReader::Service::kNis,
                      {{/*negated=*/false, NsswitchReader::Status::kSuccess,
                        NsswitchReader::Action::kReturn},
                       {/*negated=*/false, NsswitchReader::Status::kUnknown,
                        NsswitchReader::Action::kContinue}}),
                  NsswitchReader::ServiceSpecification(
                      NsswitchReader::Service::kUnknown)));
}

TEST_F(NsswitchReaderTest, IgnoresEmptyActionWithRepeatedBrackets) {
  const std::string kFile = "hosts: files [[[]]]] mdns";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kFiles),
                                   NsswitchReader::ServiceSpecification(
                                       NsswitchReader::Service::kMdns)));
}

TEST_F(NsswitchReaderTest, IgnoresEmptyActionAtEndOfString) {
  const std::string kFile = "hosts: dns [[";
  TestFileReader file_reader(kFile);
  reader_.set_file_read_call_for_testing(file_reader.GetFileReadCall());

  std::vector<NsswitchReader::ServiceSpecification> services =
      reader_.ReadAndParseHosts();

  EXPECT_THAT(services,
              testing::ElementsAre(NsswitchReader::ServiceSpecification(
                  NsswitchReader::Service::kDns)));
}

}  // namespace
}  // namespace net
