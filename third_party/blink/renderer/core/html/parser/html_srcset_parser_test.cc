// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <limits.h>

namespace blink {

typedef struct {
  float device_scale_factor;
  float effective_size;
  const char* src_input;
  const char* srcset_input;
  const char* output_url;
  float output_density;
  int output_resource_width;
} SrcsetParserTestCase;

TEST(ImageCandidateTest, Basic) {
  ImageCandidate candidate;
  ASSERT_EQ(candidate.Density(), 1);
  ASSERT_EQ(candidate.GetResourceWidth(), -1);
  ASSERT_EQ(candidate.SrcOrigin(), false);
}

TEST(HTMLSrcsetParserTest, Basic) {
  SrcsetParserTestCase test_cases[] = {
      {2.0, 0.5, "", "data:,a 1w, data:,b 2x", "data:,a", 2.0, 1},
      {2.0, 1, "", "data:,a 2w, data:,b 2x", "data:,a", 2.0, 2},
      {2.0, -1, "", "1x.gif 1x, 2x.gif 2x", "2x.gif", 2.0, -1},
      {2.0, -1, "", "1x.gif 1q, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, -1, "", "1x.gif 1q, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, -1, "", "1x.gif 1x 100h, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, -1, "", "1x.gif 1x 100w, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, -1, "", "1x.gif 1x 100h 100w, 2x.gif 2x", "2x.gif", 2.0, -1},
      {2.0, -1, "", "1x.gif 1x, 2x.gif -2x", "1x.gif", 1.0, -1},
      {2.0, -1, "", "0x.gif 0x", "0x.gif", 0.0, -1},
      {2.0, -1, "", "0x.gif -0x", "0x.gif", 0.0, -1},
      {2.0, -1, "", "neg.gif -2x", "", 1.0, -1},
      {2.0, -1, "", "1x.gif 1x, 2x.gif 2q", "1x.gif", 1.0, -1},
      {2.0, -1, "", "1x.gif, 2x.gif 2q", "1x.gif", 1.0, -1},
      {2.0, -1, "", "1x.gif  , 2x.gif 2q", "1x.gif", 1.0, -1},
      {2.0, -1, "1x.gif 1x, 2x.gif 2x", "1x.gif 1x, 2x.gif 2x", "2x.gif", 2.0,
       -1},
      {1.0, -1, "1x.gif 1x, 2x.gif 2x", "1x.gif 1x, 2x.gif 2x", "1x.gif", 1.0,
       -1},
      {1.0, -1, "1x.gif 1x, 2x.gif 2x", "", "1x.gif 1x, 2x.gif 2x", 1.0, -1},
      {2.0, -1, "src.gif", "1x.gif 1x, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, -1, "src.gif", "1x.gif 1x, 2x.gif 2x", "1x.gif", 1.0, -1},
      {1.0, -1, "src.gif", "2x.gif 2x", "src.gif", 1.0, -1},
      {2.0, -1, "src.gif", "2x.gif 2x", "2x.gif", 2.0, -1},
      {2.0, -1, "src.gif", "2x.gif 2px", "src.gif", 1.0, -1},
      {2.0, -1, "src.gif", "2x.gif 2ex", "src.gif", 1.0, -1},
      {10.0, -1, "src.gif", "2x.gif 2e1x", "2x.gif", 20.0, -1},
      {2.0, -1, "src.gif", "2x.gif 2e1x", "src.gif", 1.0, -1},
      {2.0, -1, "src.gif", "2x.gif +2x", "src.gif", 1.0, -1},
      {1.5, -1, "src.gif", "2x.gif 2x", "2x.gif", 2.0, -1},
      {2.5, -1, "src.gif", "2x.gif 2x", "2x.gif", 2.0, -1},
      {2.5, -1, "src.gif", "2x.gif 2x, 3x.gif 3x", "3x.gif", 3.0, -1},
      {2.0, -1, "", "1x,,  ,   x    ,2x  ", "1x", 1.0, -1},
      {2.0, -1, "", "1x,,  ,   x    ,2x  ", "1x", 1.0, -1},
      {2.0, -1, "", ",,1x,,  ,   x    ,2x  ", "1x", 1.0, -1},
      {2.0, -1, "", ",,1x,,", "1x", 1.0, -1},
      {2.0, -1, "", ",1x,", "1x", 1.0, -1},
      {2.0, -1, "",
       "data:image/png;base64,iVBORw0KGgoAAAANSUhEUg 1x, 2x.gif 2x", "2x.gif",
       2.0, -1},
      {2.0, -1, "",
       "data:image/png;base64,iVBORw0KGgoAAAANSUhEUg 2x, 1x.gif 1x",
       "data:image/png;base64,iVBORw0KGgoAAAANSUhEUg", 2.0, -1},
      {2.0, -1, "",
       "1x,,  ,   x    ,2x  , 1x.gif, 3x, 4x.gif 4x 100z, 5x.gif 5, dx.gif dx, "
       "2x.gif   2x ,",
       "2x.gif", 2.0, -1},
      {4.0, -1, "",
       "1x,,  ,   x    ,2x  , 1x.gif, 3x, 4x.gif 4x 100h, 5x.gif 5, dx.gif dx, "
       "2x.gif   2x ,",
       "2x.gif", 2.0, -1},
      {4.0, -1, "",
       "1x,,  ,   x    ,2x  , 1x.gif, 3x, 4x.gif 4x 100z, 5x.gif 5, dx.gif dx, "
       "2x.gif   2x ,",
       "2x.gif", 2.0, -1},
      {1.0, -1, "",
       "1x,,  ,   x    ,2x  , 1x.gif, 3x, 4x.gif 4x 100z, 5x.gif 5, dx.gif dx, "
       "2x.gif   2x ,",
       "1x", 1.0, -1},
      {5.0, -1, "",
       "1x,,  ,   x    ,2x  , 1x.gif, 3x, 4x.gif 4x 100z, 5x.gif 5, dx.gif dx, "
       "2x.gif   2x ,",
       "2x.gif", 2.0, -1},
      {2.0, -1, "",
       "1x.gif 1x, "
       "data:image/"
       "svg+xml;base64,"
       "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMDAiIGh"
       "laWdodD0iMTAwIj4KCTxyZWN0IHdpZHRoPSIxMDAiIGhlaWdodD0iMTAwIiBmaWxsPSJncm"
       "VlbiIvPgo8L3N2Zz4K 2x",
       "data:image/"
       "svg+xml;base64,"
       "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMDAiIGh"
       "laWdodD0iMTAwIj4KCTxyZWN0IHdpZHRoPSIxMDAiIGhlaWdodD0iMTAwIiBmaWxsPSJncm"
       "VlbiIvPgo8L3N2Zz4K",
       2.0, -1},
      {2.0, -1, "1x.gif",
       "data:image/"
       "svg+xml;base64,"
       "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMDAiIGh"
       "laWdodD0iMTAwIj4KCTxyZWN0IHdpZHRoPSIxMDAiIGhlaWdodD0iMTAwIiBmaWxsPSJncm"
       "VlbiIvPgo8L3N2Zz4K 2x",
       "data:image/"
       "svg+xml;base64,"
       "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxMDAiIGh"
       "laWdodD0iMTAwIj4KCTxyZWN0IHdpZHRoPSIxMDAiIGhlaWdodD0iMTAwIiBmaWxsPSJncm"
       "VlbiIvPgo8L3N2Zz4K",
       2.0, -1},
      {2.0, -1, "1x.svg#red", "1x.svg#green 2x", "1x.svg#green", 2.0, -1},
      {2.0, -1, "", "1x.svg#red 1x, 1x.svg#green 2x", "1x.svg#green", 2.0, -1},
      {1.0, 400, "", "400.gif 400w, 6000.gif 6000w", "400.gif", 1.0, 400},
      {1.0, 400, "", "400.gif 400pw, 6000.gif 6000w", "6000.gif", 15.0, 6000},
      {1.0, 400, "fallback.gif", "400.gif 400pw", "fallback.gif", 1.0, -1},
      {1.0, 400, "fallback.gif", "400.gif +400w", "fallback.gif", 1.0, -1},
      {1.0, 400, "", "400.gif 400w 400h, 6000.gif 6000w", "400.gif", 1.0, 400},
      {4.0, 400, "", "400.gif 400w, 6000.gif 6000w", "6000.gif", 15.0, 6000},
      {3.8, 400, "", "400.gif 400w, 6000.gif 6000w", "400.gif", 1.0, 400},
      {0.9, 800, "src.gif", "400.gif 400w", "400.gif", 0.5, 400},
      {0.9, 800, "src.gif", "1x.gif 1x, 400.gif 400w", "1x.gif", 1.0, -1},
      {0.9, 800, "src.gif", "1x.gif 0.6x, 400.gif 400w", "1x.gif", 0.6, -1},
      {0.9, 800, "src.gif", "1x.gif 1x, 400.gif 720w", "400.gif", 0.9, 720},
      {0.9, 800, "src.gif", "1x.gif 1x, 400.gif 719w", "1x.gif", 1.0, -1},
      {2.0, 800, "src.gif", "400.gif 400w", "400.gif", 0.5, 400},
      {1.0, 400, "src.gif", "800.gif 800w", "800.gif", 2.0, 800},
      {1.0, 400, "src.gif", "0.gif 0w, 800.gif 800w", "800.gif", 2.0, 800},
      {1.0, 400, "src.gif", "0.gif 0w, 2x.gif 2x", "src.gif", 1.0, -1},
      {1.0, 400, "src.gif", "800.gif 2x, 1600.gif 1600w", "800.gif", 2.0, -1},
      {1.0, 400, "", "400.gif 400w, 2x.gif 2x", "400.gif", 1.0, 400},
      {2.0, 400, "", "400.gif 400w, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, 0, "", "400.gif 400w, 6000.gif 6000w", "400.gif",
       std::numeric_limits<float>::infinity(), 400},
      {2.0, -1, "", ", 1x.gif 1x, 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.0, -1, "", ",1x.gif 1x, 2x.gif 2x", "1x.gif", 1.0, -1},
      {1.0, -1, "", ",1x.gif 1.x , 2x.gif 2x", "2x.gif", 2.0, -1},
      {1.2, -1, "", ",1x.gif 1x, 1.4x.gif 1.4x, 2x.gif 2x", "1.4x.gif", 1.4,
       -1},
      {1.0, -1, "", "inf.gif 0.00000000001x", "inf.gif", 1e-11, -1},
      {1.0, -1, "", "data:,a ( , data:,b 1x, ), data:,c", "data:,c", 1.0, -1},
      {1.0, 1, "", "data:,a 1w 1h", "data:,a", 1.0, 1},
      {1.0, -1, "", ",1x.gif 1x future-descriptor(3x, 4h, whatever), 2x.gif 2x",
       "2x.gif", 2.0, -1},
      {2.0, -1, "", ",1x.gif 1x future-descriptor(3x, 4h, whatever), 2x.gif 2x",
       "2x.gif", 2.0, -1},
      {1.0, -1, "", "data:,a 1 w", "", 1.0, -1},
      {1.0, -1, "", "data:,a 1  w", "", 1.0, -1},
      {1.0, -1, "", "data:,a +1x", "", 1.0, -1},
      {1.0, -1, "", "data:,a   +1x", "", 1.0, -1},
      {1.0, -1, "", "data:,a 1.0x", "data:,a", 1.0, -1},
      {1.0, -1, "", "1%20and%202.gif 1x", "1%20and%202.gif", 1.0, -1},
      {1.0, 700, "", "data:,a 0.5x, data:,b 1400w", "data:,b", 2.0, 1400},
      {0, 0, nullptr, nullptr, nullptr,
       0}  // Do not remove the terminator line.
  };

  for (unsigned i = 0; test_cases[i].src_input; ++i) {
    SrcsetParserTestCase test = test_cases[i];
    ImageCandidate candidate = BestFitSourceForImageAttributes(
        test.device_scale_factor, test.effective_size, test.src_input,
        test.srcset_input);
    ASSERT_EQ(test.output_density, candidate.Density());
    ASSERT_EQ(test.output_resource_width, candidate.GetResourceWidth());
    ASSERT_EQ(test.output_url, candidate.ToString().Ascii());
  }
}

}  // namespace blink
