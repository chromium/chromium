// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mime_sniffer.h"

#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace net {
namespace {

// This text is supposed to be representative of a plain text file the browser
// might encounter, including a variation in line lengths and blank
// lines. CRLF is used as the line-terminator to make it slightly more
// difficult. It is roughly 1KB.
const char kRepresentativePlainText[] =
    "The Tragedie of Hamlet\r\n"
    "\r\n"
    "Actus Primus. Scoena Prima.\r\n"
    "\r\n"
    "Enter Barnardo and Francisco two Centinels.\r\n"
    "\r\n"
    "  Barnardo. Who's there?\r\n"
    "  Fran. Nay answer me: Stand & vnfold\r\n"
    "your selfe\r\n"
    "\r\n"
    "   Bar. Long liue the King\r\n"
    "\r\n"
    "   Fran. Barnardo?\r\n"
    "  Bar. He\r\n"
    "\r\n"
    "   Fran. You come most carefully vpon your houre\r\n"
    "\r\n"
    "   Bar. 'Tis now strook twelue, get thee to bed Francisco\r\n"
    "\r\n"
    "   Fran. For this releefe much thankes: 'Tis bitter cold,\r\n"
    "And I am sicke at heart\r\n"
    "\r\n"
    "   Barn. Haue you had quiet Guard?\r\n"
    "  Fran. Not a Mouse stirring\r\n"
    "\r\n"
    "   Barn. Well, goodnight. If you do meet Horatio and\r\n"
    "Marcellus, the Riuals of my Watch, bid them make hast.\r\n"
    "Enter Horatio and Marcellus.\r\n"
    "\r\n"
    "  Fran. I thinke I heare them. Stand: who's there?\r\n"
    "  Hor. Friends to this ground\r\n"
    "\r\n"
    "   Mar. And Leige-men to the Dane\r\n"
    "\r\n"
    "   Fran. Giue you good night\r\n"
    "\r\n"
    "   Mar. O farwel honest Soldier, who hath relieu'd you?\r\n"
    "  Fra. Barnardo ha's my place: giue you goodnight.\r\n"
    "\r\n"
    "Exit Fran.\r\n"
    "\r\n"
    "  Mar. Holla Barnardo\r\n"
    "\r\n"
    "   Bar. Say, what is Horatio there?\r\n"
    "  Hor. A peece of him\r\n"
    "\r\n"
    "   Bar. Welcome Horatio, welcome good Marcellus\r\n"
    "\r\n";

void RunLooksLikeBinary(const std::string& plaintext, size_t iterations) {
  bool looks_like_binary = false;
  for (size_t i = 0; i < iterations; ++i) {
    if (LooksLikeBinary(plaintext))
      looks_like_binary = true;
  }
  CHECK(!looks_like_binary);
}

TEST(MimeSnifferTest, PlainTextPerfTest) {
  // Android systems have a relatively small CPU cache (512KB to 2MB).
  // It is better if the test data fits in cache so that we are not just
  // testing bus bandwidth.
  const size_t kTargetSize = 1 << 18;  // 256KB
  const size_t kWarmupIterations = 16;
  const size_t kMeasuredIterations = 1 << 15;
  std::string plaintext = kRepresentativePlainText;
  size_t expected_size = plaintext.size() << base::bits::Log2Ceiling(
                             kTargetSize / plaintext.size());
  plaintext.reserve(expected_size);
  while (plaintext.size() < kTargetSize)
    plaintext += plaintext;
  DCHECK_EQ(expected_size, plaintext.size());
  RunLooksLikeBinary(plaintext, kWarmupIterations);
  base::ElapsedTimer elapsed_timer;
  RunLooksLikeBinary(plaintext, kMeasuredIterations);
  perf_test::PerfResultReporter reporter("MimeSniffer.", "PlainText");
  reporter.RegisterImportantMetric("throughput",
                                   "bytesPerSecond_biggerIsBetter");
  reporter.AddResult("throughput", static_cast<int64_t>(plaintext.size()) *
                                       kMeasuredIterations /
                                       elapsed_timer.Elapsed().InSecondsF());
}

}  // namespace
}  // namespace net
