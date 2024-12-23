// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/cookie_access_filter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CookieAccessType, BitwiseOrOperator) {
  ASSERT_EQ(DIPSDataAccessType::kRead,
            DIPSDataAccessType::kNone | DIPSDataAccessType::kRead);

  ASSERT_EQ(DIPSDataAccessType::kWrite,
            DIPSDataAccessType::kNone | DIPSDataAccessType::kWrite);

  ASSERT_EQ(DIPSDataAccessType::kReadWrite,
            DIPSDataAccessType::kRead | DIPSDataAccessType::kWrite);

  ASSERT_EQ(DIPSDataAccessType::kUnknown,
            DIPSDataAccessType::kUnknown | DIPSDataAccessType::kNone);

  ASSERT_EQ(DIPSDataAccessType::kUnknown,
            DIPSDataAccessType::kUnknown | DIPSDataAccessType::kRead);

  ASSERT_EQ(DIPSDataAccessType::kUnknown,
            DIPSDataAccessType::kUnknown | DIPSDataAccessType::kWrite);
}

TEST(CookieAccessFilter, NoAccesses) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kNone,
                                           DIPSDataAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Former) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kRead,
                                           DIPSDataAccessType::kNone));
}

TEST(CookieAccessFilter, OneRead_Latter) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kNone,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, OneWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url2, CookieOperation::kChange);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kNone,
                                           DIPSDataAccessType::kWrite));
}

TEST(CookieAccessFilter, UnexpectedURL) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(GURL("http://other.com"), CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_FALSE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kUnknown,
                                           DIPSDataAccessType::kUnknown));
}

TEST(CookieAccessFilter, TwoReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kRead,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReadBeforeWrite_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kWrite,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWrites_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kWrite,
                                           DIPSDataAccessType::kWrite,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kRead,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceReads_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kRead,
                                           DIPSDataAccessType::kRead,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, CoalesceWriteBeforeRead_Repeated) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url1, url2}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kRead));
}

TEST(CookieAccessFilter, SameURLTwiceWithDifferentAccessTypes) {
  GURL url1("http://example.com");
  GURL url2("http://google.com");
  CookieAccessFilter filter;
  filter.AddAccess(url1, CookieOperation::kChange);
  filter.AddAccess(url2, CookieOperation::kRead);
  filter.AddAccess(url2, CookieOperation::kChange);
  filter.AddAccess(url1, CookieOperation::kRead);

  std::vector<DIPSDataAccessType> result;
  ASSERT_TRUE(filter.Filter({url1, url2, url1}, &result));
  EXPECT_THAT(result, testing::ElementsAre(DIPSDataAccessType::kWrite,
                                           DIPSDataAccessType::kReadWrite,
                                           DIPSDataAccessType::kRead));
}
