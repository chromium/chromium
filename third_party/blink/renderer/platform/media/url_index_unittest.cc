// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/url_index.h"

#include <stdint.h>

#include <list>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/base/media_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class UrlIndexTest : public testing::Test {
 public:
  UrlIndexTest() = default;

  scoped_refptr<UrlData> GetByUrl(const GURL& gurl,
                                  UrlData::CorsMode cors_mode) {
    scoped_refptr<UrlData> ret =
        url_index_.GetByUrl(gurl, cors_mode, UrlData::kNormal);
    EXPECT_EQ(ret->url(), gurl);
    EXPECT_EQ(ret->cors_mode(), cors_mode);
    return ret;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  UrlIndex url_index_{nullptr, task_environment_.GetMainThreadTaskRunner()};
};

TEST_F(UrlIndexTest, SimpleTest) {
  GURL url1("http://foo.bar.com");
  GURL url2("http://foo.bar.com/urgel");
  scoped_refptr<UrlData> a = GetByUrl(url1, UrlData::CORS_UNSPECIFIED);
  // Make sure it's valid, we still shouldn't get the same one.
  a->Use();
  a->set_range_supported();
  EXPECT_TRUE(a->Valid());
  EXPECT_EQ(a, url_index_.TryInsert(a));
  scoped_refptr<UrlData> b = GetByUrl(url1, UrlData::CORS_ANONYMOUS);
  b->Use();
  b->set_range_supported();
  EXPECT_TRUE(b->Valid());
  EXPECT_EQ(b, url_index_.TryInsert(b));
  scoped_refptr<UrlData> c = GetByUrl(url1, UrlData::CORS_USE_CREDENTIALS);
  c->Use();
  c->set_range_supported();
  EXPECT_TRUE(c->Valid());
  EXPECT_EQ(c, url_index_.TryInsert(c));
  scoped_refptr<UrlData> d = GetByUrl(url2, UrlData::CORS_UNSPECIFIED);
  d->Use();
  d->set_range_supported();
  EXPECT_TRUE(d->Valid());
  EXPECT_EQ(d, url_index_.TryInsert(d));

  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, d);
  EXPECT_NE(b, c);
  EXPECT_NE(b, d);
  EXPECT_NE(c, d);
  EXPECT_EQ(a, GetByUrl(url1, UrlData::CORS_UNSPECIFIED));
  EXPECT_EQ(b, GetByUrl(url1, UrlData::CORS_ANONYMOUS));
  EXPECT_EQ(c, GetByUrl(url1, UrlData::CORS_USE_CREDENTIALS));
  EXPECT_EQ(d, GetByUrl(url2, UrlData::CORS_UNSPECIFIED));
}

TEST_F(UrlIndexTest, UrlDataTest) {
  GURL url("http://foo.bar.com");
  scoped_refptr<UrlData> a = GetByUrl(url, UrlData::CORS_UNSPECIFIED);

  // Check default values.
  EXPECT_FALSE(a->range_supported());
  EXPECT_FALSE(a->cacheable());
  EXPECT_EQ(a->length(), kPositionNotSpecified);
  EXPECT_FALSE(a->Valid());

  a->set_length(117);
  EXPECT_EQ(a->length(), 117);

  a->set_cacheable(true);
  EXPECT_TRUE(a->cacheable());

  base::Time now = base::Time::Now();
  base::Time valid_until = now + base::Seconds(500);
  a->set_valid_until(valid_until);
  a->set_range_supported();
  EXPECT_EQ(valid_until, a->valid_until());
  EXPECT_TRUE(a->Valid());

  base::Time last_modified = now - base::Seconds(500);
  a->set_last_modified(last_modified);
  EXPECT_EQ(last_modified, a->last_modified());
}

TEST_F(UrlIndexTest, UseTest) {
  GURL url("http://foo.bar.com");
  scoped_refptr<UrlData> a = GetByUrl(url, UrlData::CORS_UNSPECIFIED);
  EXPECT_FALSE(a->Valid());
  a->Use();
  a->set_range_supported();
  EXPECT_TRUE(a->Valid());
}

TEST_F(UrlIndexTest, TryInsert) {
  GURL url("http://foo.bar.com");
  scoped_refptr<UrlData> a = GetByUrl(url, UrlData::CORS_UNSPECIFIED);
  scoped_refptr<UrlData> c = GetByUrl(url, UrlData::CORS_UNSPECIFIED);
  EXPECT_NE(a, c);
  EXPECT_FALSE(a->Valid());
  base::Time now = base::Time::Now();
  base::Time last_modified = now - base::Seconds(500);
  base::Time valid_until = now + base::Seconds(500);

  // Not sharable yet. (no ranges)
  EXPECT_EQ(a, url_index_.TryInsert(a));
  EXPECT_NE(a, GetByUrl(url, UrlData::CORS_UNSPECIFIED));
  a->set_last_modified(last_modified);

  // Not sharable yet. (no ranges)
  EXPECT_EQ(a, url_index_.TryInsert(a));
  EXPECT_NE(a, GetByUrl(url, UrlData::CORS_UNSPECIFIED));

  // Now we should be able to insert it into the index.
  a->set_range_supported();
  a->set_valid_until(valid_until);
  EXPECT_TRUE(a->Valid());
  EXPECT_EQ(a, url_index_.TryInsert(a));
  EXPECT_EQ(a, GetByUrl(url, UrlData::CORS_UNSPECIFIED));

  // |a| becomes expired...
  a->set_valid_until(now - base::Seconds(100));
  EXPECT_FALSE(a->Valid());
  scoped_refptr<UrlData> b = GetByUrl(url, UrlData::CORS_UNSPECIFIED);
  EXPECT_NE(a, b);

  b->set_range_supported();
  b->set_valid_until(valid_until);
  b->set_last_modified(last_modified);
  EXPECT_TRUE(b->Valid());

  EXPECT_EQ(b, url_index_.TryInsert(b));
  EXPECT_EQ(b, GetByUrl(url, UrlData::CORS_UNSPECIFIED));

  c->set_range_supported();
  c->set_valid_until(valid_until);
  c->set_last_modified(last_modified);
  EXPECT_TRUE(c->Valid());

  // B is still valid, so it should be preferred over C.
  EXPECT_EQ(b, url_index_.TryInsert(c));
  EXPECT_EQ(b, GetByUrl(url, UrlData::CORS_UNSPECIFIED));
}

TEST_F(UrlIndexTest, GetByUrlCacheDisabled) {
  GURL url("http://foo.bar.com");
  UrlData::CorsMode cors = UrlData::CORS_UNSPECIFIED;

  scoped_refptr<UrlData> url_data =
      url_index_.GetByUrl(url, cors, UrlData::kNormal);
  url_data->Use();
  url_data->set_range_supported();
  EXPECT_TRUE(url_data->Valid());
  url_index_.TryInsert(url_data);

  EXPECT_EQ(url_data, url_index_.GetByUrl(url, cors, UrlData::kNormal));
  EXPECT_NE(url_data, url_index_.GetByUrl(url, cors, UrlData::kCacheDisabled));
}

}  // namespace blink
