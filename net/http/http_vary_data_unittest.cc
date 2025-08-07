// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_vary_data.h"

#include <algorithm>
#include <array>

#include "base/pickle.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

typedef testing::Test HttpVaryDataTest;
using ExtraHeaders = std::vector<std::pair<std::string, std::string>>;

struct TestTransaction {
  HttpRequestInfo request;
  scoped_refptr<HttpResponseHeaders> response;

  void Init(const ExtraHeaders& request_headers,
            const std::string& response_headers) {
    std::string temp(response_headers);
    std::replace(temp.begin(), temp.end(), '\n', '\0');
    response = base::MakeRefCounted<HttpResponseHeaders>(temp);

    request.extra_headers.Clear();
    for (const auto& [key, value] : request_headers)
      request.extra_headers.SetHeader(key, value);
  }
};

}  // namespace

TEST(HttpVaryDataTest, IsInvalid) {
  // Only first of these result in an invalid vary data object.
  const auto kTestResponses = std::to_array<const char*>({
      "HTTP/1.1 200 OK\n\n",
      "HTTP/1.1 200 OK\nVary: *\n\n",
      "HTTP/1.1 200 OK\nVary: cookie, *, bar\n\n",
      "HTTP/1.1 200 OK\nVary: cookie\nFoo: 1\nVary: *\n\n",
  });

  const auto kExpectedValid = std::to_array<bool>({false, true, true, true});

  for (size_t i = 0; i < std::size(kTestResponses); ++i) {
    TestTransaction t;
    t.Init(/*request_headers=*/{}, kTestResponses[i]);

    HttpVaryData v;
    EXPECT_FALSE(v.is_valid());
    EXPECT_EQ(kExpectedValid[i], v.Init(t.request, *t.response.get()));
    EXPECT_EQ(kExpectedValid[i], v.is_valid());
  }
}

TEST(HttpVaryDataTest, MultipleInit) {
  HttpVaryData v;

  // Init to something valid.
  TestTransaction t1;
  t1.Init({{"Foo", "1"}, {"bar", "23"}}, "HTTP/1.1 200 OK\nVary: foo, bar\n\n");
  EXPECT_TRUE(v.Init(t1.request, *t1.response.get()));
  EXPECT_TRUE(v.is_valid());

  // Now overwrite by initializing to something invalid.
  TestTransaction t2;
  t2.Init({{"Foo", "1"}, {"bar", "23"}}, "HTTP/1.1 200 OK\n\n");
  EXPECT_FALSE(v.Init(t2.request, *t2.response.get()));
  EXPECT_FALSE(v.is_valid());
}

TEST(HttpVaryDataTest, DoesVary) {
  TestTransaction a;
  a.Init({{"Foo", "1"}}, "HTTP/1.1 200 OK\nVary: foo\n\n");

  TestTransaction b;
  b.Init({{"Foo", "2"}}, "HTTP/1.1 200 OK\nVary: foo\n\n");

  HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response.get()));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response.get()));
}

TEST(HttpVaryDataTest, DoesVary2) {
  TestTransaction a;
  a.Init({{"Foo", "1"}, {"bar", "23"}}, "HTTP/1.1 200 OK\nVary: foo, bar\n\n");

  TestTransaction b;
  b.Init({{"Foo", "12"}, {"bar", "3"}}, "HTTP/1.1 200 OK\nVary: foo, bar\n\n");

  HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response.get()));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response.get()));
}

TEST(HttpVaryDataTest, DoesVaryStar) {
  // Vary: * varies even when headers are identical
  const ExtraHeaders kRequestHeaders = {{"Foo", "1"}};
  const char kResponse[] = "HTTP/1.1 200 OK\nVary: *\n\n";

  TestTransaction a;
  a.Init(kRequestHeaders, kResponse);

  TestTransaction b;
  b.Init(kRequestHeaders, kResponse);

  HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response.get()));

  EXPECT_FALSE(v.MatchesRequest(b.request, *b.response.get()));
}

TEST(HttpVaryDataTest, DoesntVary) {
  TestTransaction a;
  a.Init({{"Foo", "1"}}, "HTTP/1.1 200 OK\nVary: foo\n\n");

  TestTransaction b;
  b.Init({{"Foo", "1"}}, "HTTP/1.1 200 OK\nVary: foo\n\n");

  HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response.get()));

  EXPECT_TRUE(v.MatchesRequest(b.request, *b.response.get()));
}

TEST(HttpVaryDataTest, DoesntVary2) {
  TestTransaction a;
  a.Init({{"Foo", "1"}, {"bAr", "2"}}, "HTTP/1.1 200 OK\nVary: foo, bar\n\n");

  TestTransaction b;
  b.Init({{"Foo", "1"}, {"baR", "2"}},
         "HTTP/1.1 200 OK\nVary: foo\nVary: bar\n\n");

  HttpVaryData v;
  EXPECT_TRUE(v.Init(a.request, *a.response.get()));

  EXPECT_TRUE(v.MatchesRequest(b.request, *b.response.get()));
}

TEST(HttpVaryDataTest, DoesntVaryByCookieForRedirect) {
  TestTransaction a;
  a.Init({{"Cookie", "1"}}, "HTTP/1.1 301 Moved\nLocation: x\n\n");

  HttpVaryData v;
  EXPECT_FALSE(v.Init(a.request, *a.response.get()));
}

TEST(HttpVaryDataTest, PersistAndLoadMD5) {
  const ExtraHeaders kRequestHeaders = {{"Foo", "1"}};
  const char kResponse[] = "HTTP/1.1 200 OK\nVary: Foo\n\n";

  TestTransaction a;
  a.Init(kRequestHeaders, kResponse);

  HttpVaryData vary_data;
  ASSERT_TRUE(vary_data.Init(a.request, *a.response.get(),
                             HttpVaryData::HashType::kMD5));
  EXPECT_TRUE(vary_data.is_valid());

  base::Pickle pickle;
  vary_data.Persist(&pickle);

  HttpVaryData new_vary_data;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(new_vary_data.InitFromPickle(&iter));
  EXPECT_TRUE(new_vary_data.is_valid());
  EXPECT_EQ(HttpVaryData::HashType::kMD5, new_vary_data.hash_type());
}

TEST(HttpVaryDataTest, PersistAndLoadSHA256) {
  const ExtraHeaders kRequestHeaders = {{"Foo", "1"}};
  const char kResponse[] = "HTTP/1.1 200 OK\nVary: Foo\n\n";

  TestTransaction a;
  a.Init(kRequestHeaders, kResponse);

  HttpVaryData vary_data;
  ASSERT_TRUE(vary_data.Init(a.request, *a.response.get(),
                             HttpVaryData::HashType::kSHA256));
  EXPECT_TRUE(vary_data.is_valid());

  base::Pickle pickle;
  vary_data.Persist(&pickle);

  HttpVaryData new_vary_data;
  base::PickleIterator iter(pickle);
  EXPECT_TRUE(new_vary_data.InitFromPickle(&iter));
  EXPECT_TRUE(new_vary_data.is_valid());
  EXPECT_EQ(HttpVaryData::HashType::kSHA256, new_vary_data.hash_type());
}

TEST(HttpVaryDataTest, PersistOldFormatAsNewFormat) {
  // Create a pickle with an old-format raw MD5 hash.
  base::Pickle old_pickle;
  Md5Hash md5_hash = {1, 2, 3};
  old_pickle.WriteBytes(md5_hash);

  // Load it.
  HttpVaryData vary_data;
  base::PickleIterator iter(old_pickle);
  ASSERT_TRUE(vary_data.InitFromPickle(&iter));
  EXPECT_EQ(HttpVaryData::HashType::kMD5, vary_data.hash_type());

  // Persist it to a new pickle.
  base::Pickle new_pickle;
  vary_data.Persist(&new_pickle);

  // Verify that the new pickle is in the new format.
  base::PickleIterator new_iter(new_pickle);
  int hash_type;
  ASSERT_TRUE(new_iter.ReadInt(&hash_type));
  EXPECT_EQ(static_cast<int>(HttpVaryData::HashType::kMD5), hash_type);
  std::optional<base::span<const uint8_t>> bytes =
      new_iter.ReadBytes(crypto::obsolete::Md5::kSize);
  ASSERT_TRUE(bytes);
  EXPECT_EQ(base::span(md5_hash), *bytes);
}

TEST(HttpVaryDataTest, MatchesRequestMD5) {
  HttpRequestInfo request_info;
  request_info.extra_headers.SetHeader("accept-language", "en-US");
  std::string raw_headers = "HTTP/1.1 200 OK\nVary: accept-language\n\n";
  std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
  auto response_headers =
      base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  HttpVaryData vary_data;
  ASSERT_TRUE(vary_data.Init(request_info, *response_headers,
                             HttpVaryData::HashType::kMD5));

  // Matching request.
  EXPECT_TRUE(vary_data.MatchesRequest(request_info, *response_headers));

  // Mismatched request.
  HttpRequestInfo other_request_info;
  other_request_info.extra_headers.SetHeader("accept-language", "en-GB");
  EXPECT_FALSE(vary_data.MatchesRequest(other_request_info, *response_headers));
}

TEST(HttpVaryDataTest, MatchesRequestSHA256) {
  HttpRequestInfo request_info;
  request_info.extra_headers.SetHeader("accept-language", "en-US");
  std::string raw_headers = "HTTP/1.1 200 OK\nVary: accept-language\n\n";
  std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
  auto response_headers =
      base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  HttpVaryData vary_data;
  ASSERT_TRUE(vary_data.Init(request_info, *response_headers,
                             HttpVaryData::HashType::kSHA256));

  // Matching request.
  EXPECT_TRUE(vary_data.MatchesRequest(request_info, *response_headers));

  // Mismatched request.
  HttpRequestInfo other_request_info;
  other_request_info.extra_headers.SetHeader("accept-language", "en-GB");
  EXPECT_FALSE(vary_data.MatchesRequest(other_request_info, *response_headers));
}

TEST(HttpVaryDataTest, InitFromInvalidPickle) {
  // An empty pickle is invalid.
  {
    HttpVaryData vary_data;
    base::Pickle pickle;
    base::PickleIterator iter(pickle);
    EXPECT_FALSE(vary_data.InitFromPickle(&iter));
  }

  // A pickle with random data is invalid.
  {
    base::Pickle pickle;
    pickle.WriteInt(12345);
    pickle.WriteString("foo");
    base::PickleIterator iter(pickle);
    HttpVaryData new_vary_data;
    EXPECT_FALSE(new_vary_data.InitFromPickle(&iter));
  }
}

}  // namespace net
