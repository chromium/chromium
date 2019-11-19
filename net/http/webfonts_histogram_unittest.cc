// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/webfonts_histogram.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace web_fonts_histogram {

namespace {

const char kRobotoHistogramName[] = "WebFont.HttpCacheStatus_roboto";
const char kOpenSansHistogramName[] = "WebFont.HttpCacheStatus_opensans";
const char kOthersHistogramName[] = "WebFont.HttpCacheStatus_others";

const char kHttps[] = "https://";
const char kHttp[] = "http://";

const char kPattern1[] = "themes.googleusercontent.com/static/fonts/";
const char kPattern2[] = "ssl.gstatic.com/fonts/";
const char kPattern3[] = "fonts.gstatic.com/s/";

const char kRoboto[] = "roboto";
const char kOpenSans[] = "opensans";

}  // namespace

TEST(WebfontsHistogramTest, EmptyKey_NoRecord) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED, "");
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, RecordRoboto) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED,
                         base::StrCat({kHttps, kPattern1, kRoboto}));
  histograms.ExpectUniqueSample(kRobotoHistogramName, 3 /* ENTRY_USED */, 1);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, RecordOpenSans) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_NOT_IN_CACHE,
                         base::StrCat({kHttp, kPattern2, kOpenSans}));
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectUniqueSample(kOpenSansHistogramName,
                                2 /* ENTRY_NOT_IN_CACHE */, 1);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, EmptyFont_RecordOthers) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_CANT_CONDITIONALIZE,
                         base::StrCat({kHttps, kPattern3}));
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectUniqueSample(kOthersHistogramName,
                                6 /* ENTRY_CANT_CONDITIONALIZE */, 1);
}

TEST(WebfontsHistogramTest, ArbitraryFont_RecordOthers) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_OTHER,
                         base::StrCat({kHttps, kPattern2, "abc"}));
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectUniqueSample(kOthersHistogramName, 1 /* ENTRY_OTHER */, 1);
}

TEST(WebfontsHistogramTest, WithSuffix_Record) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED,
                         base::StrCat({kHttps, kPattern1, kRoboto, "abc"}));
  histograms.ExpectUniqueSample(kRobotoHistogramName, 3 /* ENTRY_USED */, 1);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, WithPrefix_NoRecord) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED,
                         base::StrCat({"abc", kHttps, kPattern1, kRoboto}));
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, OtherProtocol_NoRecord) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_OTHER,
                         base::StrCat({"ftp://", kPattern1, kRoboto}));
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, OtherPattern_NoRecord) {
  base::HistogramTester histograms;
  MaybeRecordCacheStatus(
      HttpResponseInfo::ENTRY_USED,
      base::StrCat({kHttps, "fonts.gstatic.com//s/", kRoboto}));
  histograms.ExpectTotalCount(kRobotoHistogramName, 0);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 0);
  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

TEST(WebfontsHistogramTest, TwoRobotoSameBucket_TwoOpenSansDifferentBucket) {
  base::HistogramTester histograms;

  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED,
                         base::StrCat({kHttps, kPattern2, kRoboto}));
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED,
                         base::StrCat({kHttp, kPattern3, kRoboto}));
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_USED,
                         base::StrCat({kHttps, kPattern2, kOpenSans}));
  MaybeRecordCacheStatus(HttpResponseInfo::ENTRY_OTHER,
                         base::StrCat({kHttp, kPattern3, kOpenSans}));

  histograms.ExpectUniqueSample(kRobotoHistogramName, 3 /* ENTRY_USED */, 2);
  histograms.ExpectTotalCount(kRobotoHistogramName, 2);

  histograms.ExpectBucketCount(kOpenSansHistogramName, 3 /* ENTRY_USED */, 1);
  histograms.ExpectBucketCount(kOpenSansHistogramName, 1 /* ENTRY_OTHER */, 1);
  histograms.ExpectTotalCount(kOpenSansHistogramName, 2);

  histograms.ExpectTotalCount(kOthersHistogramName, 0);
}

}  // namespace web_fonts_histogram
}  // namespace net
