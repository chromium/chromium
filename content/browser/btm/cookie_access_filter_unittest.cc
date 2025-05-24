// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/cookie_access_filter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(CookieAccessType, BitwiseOrOperator) {
  ASSERT_EQ(BtmDataAccessType::kRead,
            BtmDataAccessType::kNone | BtmDataAccessType::kRead);

  ASSERT_EQ(BtmDataAccessType::kWrite,
            BtmDataAccessType::kNone | BtmDataAccessType::kWrite);

  ASSERT_EQ(BtmDataAccessType::kReadWrite,
            BtmDataAccessType::kRead | BtmDataAccessType::kWrite);

  ASSERT_EQ(BtmDataAccessType::kUnknown,
            BtmDataAccessType::kUnknown | BtmDataAccessType::kNone);

  ASSERT_EQ(BtmDataAccessType::kUnknown,
            BtmDataAccessType::kUnknown | BtmDataAccessType::kRead);

  ASSERT_EQ(BtmDataAccessType::kUnknown,
            BtmDataAccessType::kUnknown | BtmDataAccessType::kWrite);
}

TEST(CookieAccessFilter, NoAccesses) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kNone,
                                           BtmDataAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Former) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kRead,
                                           BtmDataAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Latter) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kNone,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, OneWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieOperation::kChange);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kNone,
                                           BtmDataAccessType::kWrite));
}

TEST(CookieAccessFilter, UnexpectedURL) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(GURL("http://other.com"), CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kUnknown,
                                           BtmDataAccessType::kUnknown));
}

TEST(CookieAccessFilter, TwoReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kRead,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kWrite,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kWrite,
                                           BtmDataAccessType::kWrite,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kRead,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kRead,
                                           BtmDataAccessType::kRead,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kRead));
}

TEST(CookieAccessFilter, SameURLTwiceWithDifferentAccessTypes) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);

  std::vector<BtmDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2, url1}, result));
  EXPECT_THAT(result, testing::ElementsAre(BtmDataAccessType::kWrite,
                                           BtmDataAccessType::kReadWrite,
                                           BtmDataAccessType::kRead));
}

}  // namespace content
