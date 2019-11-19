// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_source_list.h"
#include "content/common/content_security_policy/csp_context.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Allow() is an abbreviation of CSPSourceList::Allow(). Useful for writting
// test expectations on one line.
bool Allow(const CSPSourceList& source_list,
           const GURL& url,
           CSPContext* context,
           bool is_redirect = false,
           bool is_response_check = false) {
  return CSPSourceList::Allow(source_list, url, context, is_redirect,
                              is_response_check);
}

}  // namespace

TEST(CSPSourceList, MultipleSource) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  CSPSourceList source_list(
      false,  // allow_self
      false,  // allow_star
      false,  // allow_redirects
      {CSPSource("", "a.com", false, url::PORT_UNSPECIFIED, false, ""),
       CSPSource("", "b.com", false, url::PORT_UNSPECIFIED, false, "")});
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("http://c.com"), &context));
}

TEST(CSPSourceList, AllowStar) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  CSPSourceList source_list(false,                      // allow_self
                            true,                       // allow_star
                            false,                      // allow_redirects
                            std::vector<CSPSource>());  // source_list
  EXPECT_TRUE(Allow(source_list, GURL("http://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("ws://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("wss://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("ftp://not-example.com"), &context));

  EXPECT_FALSE(Allow(source_list, GURL("file://not-example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), &context));

  // With a protocol of 'file', '*' allow 'file:'
  context.SetSelf(url::Origin::Create(GURL("file://example.com")));
  EXPECT_TRUE(Allow(source_list, GURL("file://not-example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("applewebdata://a.test"), &context));
}

TEST(CSPSourceList, AllowSelf) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  CSPSourceList source_list(true,                       // allow_self
                            false,                      // allow_star
                            false,                      // allow_redirects
                            std::vector<CSPSource>());  // source_list
  EXPECT_TRUE(Allow(source_list, GURL("http://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("http://not-example.com"), &context));
  EXPECT_TRUE(Allow(source_list, GURL("https://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("ws://example.com"), &context));
}

TEST(CSPSourceList, AllowStarAndSelf) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("https://a.com")));
  CSPSourceList source_list(false,  // allow_self
                            false,  // allow_star
                            false,  // allow_redirects
                            std::vector<CSPSource>());

  // If the request is allowed by {*} and not by {'self'} then it should be
  // allowed by the union {*,'self'}.
  source_list.allow_self = true;
  source_list.allow_star = false;
  EXPECT_FALSE(Allow(source_list, GURL("http://b.com"), &context));
  source_list.allow_self = false;
  source_list.allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
  source_list.allow_self = true;
  source_list.allow_star = true;
  EXPECT_TRUE(Allow(source_list, GURL("http://b.com"), &context));
}

TEST(CSPSourceList, AllowSelfWithUnspecifiedPort) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GetWebUIURL("print")));
  CSPSourceList source_list(true,                       // allow_self
                            false,                      // allow_star
                            false,                      // allow_redirects
                            std::vector<CSPSource>());  // source_list

  EXPECT_TRUE(Allow(source_list,
                    GURL(GetWebUIURLString("print/pdf/index.html?") +
                         GetWebUIURLString("print/1/0/print.pdf")),
                    &context));
}

TEST(CSPSourceList, AllowNone) {
  CSPContext context;
  context.SetSelf(url::Origin::Create(GURL("http://example.com")));
  CSPSourceList source_list(false,                      // allow_self
                            false,                      // allow_star
                            false,                      // allow_redirects
                            std::vector<CSPSource>());  // source_list
  EXPECT_FALSE(Allow(source_list, GURL("http://example.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("https://example.test/"), &context));
}

TEST(CSPSourceTest, SelfIsUnique) {
  // Policy: 'self'
  CSPSourceList source_list(true,                       // allow_self
                            false,                      // allow_star
                            false,                      // allow_redirects
                            std::vector<CSPSource>());  // source_list
  CSPContext context;

  context.SetSelf(url::Origin::Create(GURL("http://a.com")));
  EXPECT_TRUE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("data:text/html,hello"), &context));

  context.SetSelf(
      url::Origin::Create(GURL("data:text/html,<iframe src=[...]>")));
  EXPECT_FALSE(Allow(source_list, GURL("http://a.com"), &context));
  EXPECT_FALSE(Allow(source_list, GURL("data:text/html,hello"), &context));
}

}  // namespace content
