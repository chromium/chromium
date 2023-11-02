// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/range_set.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

TEST(RangeSetTest, Union) {
  {
    RangeSet range_set;
    EXPECT_EQ("{}", range_set.ToString());
    range_set.Union(gfx::Range(50, 100));
    EXPECT_EQ("{[50,100)}", range_set.ToString());
    range_set.Union(gfx::Range(80, 150));
    EXPECT_EQ("{[50,150)}", range_set.ToString());
    range_set.Union(gfx::Range(0, 70));
    EXPECT_EQ("{[0,150)}", range_set.ToString());
    range_set.Union(gfx::Range(70, 120));
    EXPECT_EQ("{[0,150)}", range_set.ToString());
    range_set.Union(gfx::Range(200, 150));
    EXPECT_EQ("{[0,150)[151,201)}", range_set.ToString());
    range_set.Union(gfx::Range(150, 151));
    EXPECT_EQ("{[0,201)}", range_set.ToString());
    range_set.Union(gfx::Range(0, 300));
    EXPECT_EQ("{[0,300)}", range_set.ToString());
    range_set.Union(gfx::Range(500, 600));
    EXPECT_EQ("{[0,300)[500,600)}", range_set.ToString());
  }
  {
    RangeSet range_set_1;
    range_set_1.Union(gfx::Range(0, 10));
    range_set_1.Union(gfx::Range(20, 30));
    range_set_1.Union(gfx::Range(40, 50));

    EXPECT_EQ("{[0,10)[20,30)[40,50)}", range_set_1.ToString());
    range_set_1.Union(range_set_1);
    EXPECT_EQ("{[0,10)[20,30)[40,50)}", range_set_1.ToString());

    RangeSet range_set_2;
    range_set_2.Union(gfx::Range(10, 20));
    range_set_2.Union(gfx::Range(30, 40));
    range_set_2.Union(gfx::Range(50, 60));

    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set_2.ToString());
    range_set_1.Union(range_set_2);
    EXPECT_EQ("{[0,60)}", range_set_1.ToString());
    EXPECT_EQ(RangeSet(gfx::Range(0, 60)), range_set_1);
  }
}

TEST(RangeSetTest, Contains) {
  RangeSet range_set;
  range_set.Union(gfx::Range(10, 20));
  range_set.Union(gfx::Range(30, 40));
  range_set.Union(gfx::Range(50, 60));
  EXPECT_TRUE(range_set.Contains(range_set));

  {
    EXPECT_FALSE(range_set.Contains(9));
    EXPECT_FALSE(range_set.Contains(29));
    EXPECT_FALSE(range_set.Contains(49));

    EXPECT_TRUE(range_set.Contains(10));
    EXPECT_TRUE(range_set.Contains(30));
    EXPECT_TRUE(range_set.Contains(50));

    EXPECT_TRUE(range_set.Contains(15));
    EXPECT_TRUE(range_set.Contains(35));
    EXPECT_TRUE(range_set.Contains(55));

    EXPECT_TRUE(range_set.Contains(19));
    EXPECT_TRUE(range_set.Contains(39));
    EXPECT_TRUE(range_set.Contains(59));

    EXPECT_FALSE(range_set.Contains(20));
    EXPECT_FALSE(range_set.Contains(40));
    EXPECT_FALSE(range_set.Contains(60));
  }
  {
    EXPECT_FALSE(range_set.Contains(gfx::Range(0, 10)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(20, 30)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(40, 50)));

    EXPECT_FALSE(range_set.Contains(gfx::Range(5, 15)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(25, 35)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(45, 55)));

    EXPECT_TRUE(range_set.Contains(gfx::Range(10, 15)));
    EXPECT_TRUE(range_set.Contains(gfx::Range(30, 35)));
    EXPECT_TRUE(range_set.Contains(gfx::Range(50, 55)));

    EXPECT_TRUE(range_set.Contains(gfx::Range(15, 20)));
    EXPECT_TRUE(range_set.Contains(gfx::Range(35, 40)));
    EXPECT_TRUE(range_set.Contains(gfx::Range(55, 60)));

    EXPECT_TRUE(range_set.Contains(gfx::Range(10, 20)));
    EXPECT_TRUE(range_set.Contains(gfx::Range(30, 40)));
    EXPECT_TRUE(range_set.Contains(gfx::Range(50, 60)));

    EXPECT_FALSE(range_set.Contains(gfx::Range(15, 25)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(35, 45)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(55, 65)));

    EXPECT_FALSE(range_set.Contains(gfx::Range(20, 25)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(40, 45)));
    EXPECT_FALSE(range_set.Contains(gfx::Range(60, 65)));

    EXPECT_FALSE(range_set.Contains(gfx::Range(0, 100)));
  }
  {
    RangeSet range_set_2 = range_set;
    EXPECT_TRUE(range_set_2.Contains(range_set));
    range_set_2.Union(gfx::Range(100, 200));
    EXPECT_TRUE(range_set_2.Contains(range_set));
    EXPECT_FALSE(range_set.Contains(range_set_2));
  }
}

TEST(RangeSetTest, Intersects) {
  RangeSet range_set;
  range_set.Union(gfx::Range(10, 20));
  range_set.Union(gfx::Range(30, 40));
  range_set.Union(gfx::Range(50, 60));
  EXPECT_TRUE(range_set.Intersects(range_set));
  {
    EXPECT_FALSE(range_set.Intersects(gfx::Range(0, 10)));
    EXPECT_FALSE(range_set.Intersects(gfx::Range(20, 30)));
    EXPECT_FALSE(range_set.Intersects(gfx::Range(40, 50)));

    EXPECT_TRUE(range_set.Intersects(gfx::Range(5, 15)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(25, 35)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(45, 55)));

    EXPECT_TRUE(range_set.Intersects(gfx::Range(10, 15)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(30, 35)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(50, 55)));

    EXPECT_TRUE(range_set.Intersects(gfx::Range(15, 20)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(35, 40)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(55, 60)));

    EXPECT_TRUE(range_set.Intersects(gfx::Range(10, 20)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(30, 40)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(50, 60)));

    EXPECT_TRUE(range_set.Intersects(gfx::Range(15, 25)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(35, 45)));
    EXPECT_TRUE(range_set.Intersects(gfx::Range(55, 65)));

    EXPECT_FALSE(range_set.Intersects(gfx::Range(20, 25)));
    EXPECT_FALSE(range_set.Intersects(gfx::Range(40, 45)));
    EXPECT_FALSE(range_set.Intersects(gfx::Range(60, 65)));

    EXPECT_TRUE(range_set.Intersects(gfx::Range(0, 100)));
  }
  {
    RangeSet range_set_2;
    range_set_2.Union(gfx::Range(5, 15));
    range_set_2.Union(gfx::Range(25, 35));
    range_set_2.Union(gfx::Range(45, 55));
    EXPECT_TRUE(range_set_2.Intersects(range_set));
  }
  {
    RangeSet range_set_2;
    range_set_2.Union(gfx::Range(5, 10));
    range_set_2.Union(gfx::Range(25, 30));
    range_set_2.Union(gfx::Range(45, 50));
    EXPECT_FALSE(range_set_2.Intersects(range_set));
  }
}

TEST(RangeSetTest, Intersect) {
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));

    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Intersect(range_set);
    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Intersect(gfx::Range(0, 100));
    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Intersect(gfx::Range(0, 55));
    EXPECT_EQ("{[10,20)[30,40)[50,55)}", range_set.ToString());
    range_set.Intersect(gfx::Range(15, 100));
    EXPECT_EQ("{[15,20)[30,40)[50,55)}", range_set.ToString());
    range_set.Intersect(gfx::Range(17, 53));
    EXPECT_EQ("{[17,20)[30,40)[50,53)}", range_set.ToString());
    range_set.Intersect(gfx::Range(19, 45));
    EXPECT_EQ("{[19,20)[30,40)}", range_set.ToString());
    range_set.Intersect(gfx::Range(30, 45));
    EXPECT_EQ("{[30,40)}", range_set.ToString());
    range_set.Intersect(gfx::Range(35, 40));
    EXPECT_EQ("{[35,40)}", range_set.ToString());
    range_set.Intersect(gfx::Range(35, 35));
    EXPECT_TRUE(range_set.IsEmpty());
  }
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));

    RangeSet range_set_2;
    range_set_2.Union(gfx::Range(12, 17));
    range_set_2.Union(gfx::Range(25, 35));
    range_set_2.Union(gfx::Range(39, 55));
    range_set_2.Union(gfx::Range(59, 100));

    range_set.Intersect(range_set_2);
    EXPECT_EQ("{[12,17)[30,35)[39,40)[50,55)[59,60)}", range_set.ToString());
  }
}

TEST(RangeSetTest, Subtract) {
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));

    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(35, 35));
    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(0, 5));
    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(70, 80));
    EXPECT_EQ("{[10,20)[30,40)[50,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(35, 39));
    EXPECT_EQ("{[10,20)[30,35)[39,40)[50,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(15, 32));
    EXPECT_EQ("{[10,15)[32,35)[39,40)[50,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(15, 55));
    EXPECT_EQ("{[10,15)[55,60)}", range_set.ToString());
    range_set.Subtract(gfx::Range(0, 100));
    EXPECT_EQ("{}", range_set.ToString());
  }
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));
    range_set.Subtract(range_set);
    EXPECT_EQ("{}", range_set.ToString());
  }
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));

    RangeSet range_set_2;
    range_set_2.Union(gfx::Range(12, 17));
    range_set_2.Union(gfx::Range(25, 35));
    range_set_2.Union(gfx::Range(39, 55));
    range_set_2.Union(gfx::Range(59, 100));

    range_set.Subtract(range_set_2);
    EXPECT_EQ("{[10,12)[17,20)[35,39)[55,59)}", range_set.ToString());
  }
}

TEST(RangeSetTest, Xor) {
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));
    range_set.Xor(range_set);
    EXPECT_EQ("{}", range_set.ToString());
  }
  {
    RangeSet range_set;
    range_set.Union(gfx::Range(10, 20));
    range_set.Union(gfx::Range(30, 40));
    range_set.Union(gfx::Range(50, 60));

    RangeSet range_set_2;
    range_set_2.Union(gfx::Range(12, 17));
    range_set_2.Union(gfx::Range(25, 35));
    range_set_2.Union(gfx::Range(39, 55));
    range_set_2.Union(gfx::Range(59, 100));

    range_set.Xor(range_set_2);
    EXPECT_EQ("{[10,12)[17,20)[25,30)[35,39)[40,50)[55,59)[60,100)}",
              range_set.ToString());
  }
}

TEST(RangeSetTest, OperationsOnEmptySet) {
  RangeSet range_set;
  range_set.Intersect(gfx::Range(10, 20));
  range_set.Intersects(gfx::Range(10, 20));
  range_set.Subtract(gfx::Range(10, 20));
  range_set.Xor(gfx::Range(30, 40));
  range_set.Union(gfx::Range(10, 20));
}

}  // namespace chrome_pdf
