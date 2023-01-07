// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/nup_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(NupParametersTest, SetNupParams) {
  {
    // Set N-up parameters for 1-up, and source doc is portrait.
    NupParameters nup_params(1, false);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(1, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(1, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 1-up, and source doc is landscape.
    NupParameters nup_params(1, true);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(1, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(1, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 2-up, and source doc is portrait.
    NupParameters nup_params(2, false);
    EXPECT_TRUE(nup_params.landscape());
    EXPECT_EQ(2, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(1, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 2-up, and source doc is landscape.
    NupParameters nup_params(2, true);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(1, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(2, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 4-up, and source doc is portrait.
    NupParameters nup_params(4, false);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(2, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(2, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 4-up, and source doc is landscape.
    NupParameters nup_params(4, true);
    EXPECT_TRUE(nup_params.landscape());
    EXPECT_EQ(2, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(2, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 6-up, and source doc is portrait.
    NupParameters nup_params(6, false);
    EXPECT_TRUE(nup_params.landscape());
    EXPECT_EQ(3, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(2, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 6-up, and source doc is landscape.
    NupParameters nup_params(6, true);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(2, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(3, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 9-up, and source doc is portrait.
    NupParameters nup_params(9, false);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(3, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(3, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 9-up, and source doc is landscape.
    NupParameters nup_params(9, true);
    EXPECT_TRUE(nup_params.landscape());
    EXPECT_EQ(3, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(3, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 16-up, and source doc is portrait.
    NupParameters nup_params(16, false);
    EXPECT_FALSE(nup_params.landscape());
    EXPECT_EQ(4, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(4, nup_params.num_pages_on_y_axis());
  }

  {
    // Set N-up parameters for 16-up, and source doc is landscape.
    NupParameters nup_params(16, true);
    EXPECT_TRUE(nup_params.landscape());
    EXPECT_EQ(4, nup_params.num_pages_on_x_axis());
    EXPECT_EQ(4, nup_params.num_pages_on_y_axis());
  }
}

}  // namespace printing
