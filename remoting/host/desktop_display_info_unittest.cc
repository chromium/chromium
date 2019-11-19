// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info.h"

#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/961064): Fix memory leaks in tests and re-enable on LSAN.
#ifdef LEAK_SANITIZER
#define MAYBE_SingleDisplay DISABLED_SingleDisplay
#define MAYBE_DualDisplayRight_ReverseOrder \
  DISABLED_DualDisplayRight_ReverseOrder
#define MAYBE_DualDisplayLeft_ReverseOrder DISABLED_DualDisplayLeft_ReverseOrder
#define MAYBE_DualDisplayRight DISABLED_DualDisplayRight
#define MAYBE_DualDisplayLeft DISABLED_DualDisplayLeft
#define MAYBE_Multimon3 DISABLED_Multimon3
#define MAYBE_TripleDisplayMiddle DISABLED_TripleDisplayMiddle
#define MAYBE_Multimon7 DISABLED_Multimon7
#else
#define MAYBE_SingleDisplay SingleDisplay
#define MAYBE_DualDisplayRight_ReverseOrder DualDisplayRight_ReverseOrder
#define MAYBE_DualDisplayLeft_ReverseOrder DualDisplayLeft_ReverseOrder
#define MAYBE_DualDisplayRight DualDisplayRight
#define MAYBE_DualDisplayLeft DualDisplayLeft
#define MAYBE_Multimon3 Multimon3
#define MAYBE_TripleDisplayMiddle TripleDisplayMiddle
#define MAYBE_Multimon7 Multimon7
#endif

namespace remoting {

class DesktopDisplayInfoTest : public testing::Test {
 public:
  void AddDisplay(int x, int y, int width, int height) {
    auto* display = new DisplayGeometry();
    display->x = x;
    display->y = y;
    display->width = width;
    display->height = height;
    display->dpi = 96;
    display->bpp = 24;
    display->is_default = false;
    info_.AddDisplay(display);
  }

  void VerifyDisplayOffset(const base::Location& from_here,
                           unsigned int id,
                           int x,
                           int y) {
    webrtc::DesktopVector offset = info_.CalcDisplayOffset(id);
    EXPECT_EQ(x, offset.x()) << "Location: " << from_here.ToString();
    EXPECT_EQ(y, offset.y()) << "Location: " << from_here.ToString();
  }

 protected:
  DesktopDisplayInfo info_;
};

// o---------+
// | 0       |
// | 300x200 |
// +---------+
// o = desktop origin
TEST_F(DesktopDisplayInfoTest, MAYBE_SingleDisplay) {
  AddDisplay(0, 0, 300, 200);

  VerifyDisplayOffset(FROM_HERE, 0, 0, 0);
}

// o---------+------------+
// | 0       | 1          |
// | 300x200 | 500x400    |
// +---------+            |
//           +------------+
TEST_F(DesktopDisplayInfoTest, MAYBE_DualDisplayRight) {
  AddDisplay(0, 0, 300, 200);
  AddDisplay(300, 0, 500, 400);

  VerifyDisplayOffset(FROM_HERE, 0, 0, 0);
  VerifyDisplayOffset(FROM_HERE, 1, 300, 0);
}

// o---------+------------+
// | 1       | 0          |
// | 300x200 | 500x400    |
// +---------+            |
//           +------------+
TEST_F(DesktopDisplayInfoTest, MAYBE_DualDisplayRight_ReverseOrder) {
  AddDisplay(300, 0, 500, 400);
  AddDisplay(0, 0, 300, 200);

  VerifyDisplayOffset(FROM_HERE, 0, 300, 0);
  VerifyDisplayOffset(FROM_HERE, 1, 0, 0);
}

// +---------o------------+
// | 1       | 0          |
// | 300x200 | 500x400    |
// +---------+            |
//           +------------+
TEST_F(DesktopDisplayInfoTest, MAYBE_DualDisplayLeft) {
  AddDisplay(0, 0, 500, 400);
  AddDisplay(-300, 0, 300, 200);

  VerifyDisplayOffset(FROM_HERE, 0, 300, 0);
  VerifyDisplayOffset(FROM_HERE, 1, 0, 0);
}

// +---------o------------+
// | 0       | 1          |
// | 300x200 | 500x400    |
// +---------+            |
//           +------------+
TEST_F(DesktopDisplayInfoTest, MAYBE_DualDisplayLeft_ReverseOrder) {
  AddDisplay(-300, 0, 300, 200);
  AddDisplay(0, 0, 500, 400);

  VerifyDisplayOffset(FROM_HERE, 0, 0, 0);
  VerifyDisplayOffset(FROM_HERE, 1, 300, 0);
}

// +---------o------------+
// | 0       | 1          +---------+
// | 300x200 | 500x400    | 2       |
// +---------+            | 400x350 |
//           +------------+---------+
TEST_F(DesktopDisplayInfoTest, MAYBE_TripleDisplayMiddle) {
  AddDisplay(-300, 0, 300, 200);
  AddDisplay(0, 0, 500, 400);  // Default display.
  AddDisplay(500, 50, 400, 350);

  VerifyDisplayOffset(FROM_HERE, 0, 0, 0);
  VerifyDisplayOffset(FROM_HERE, 1, 300, 0);
  VerifyDisplayOffset(FROM_HERE, 2, 800, 50);
}

//  x         o-----------+            - 0
//            | 0         |
//  +---------+ 500x400   |            - 350
//  | 2       +-------+---+-------+    - 400
//  | 300x200 |       | 1         |
//  +---------+       | 600x450   |    - 550
//                    +-----------+    - 950
//  |         |       |   |       |
// -300       0      300 500     900
TEST_F(DesktopDisplayInfoTest, MAYBE_Multimon3) {
  AddDisplay(0, 0, 500, 400);  // Default display.
  AddDisplay(300, 400, 600, 450);
  AddDisplay(-300, 350, 300, 200);

  VerifyDisplayOffset(FROM_HERE, 0, 300, 0);
  VerifyDisplayOffset(FROM_HERE, 1, 600, 400);
  VerifyDisplayOffset(FROM_HERE, 2, 0, 350);
}

//  x                     +-------+               -- -50
//                        | 1     |
//          +-------+     | 60x40 |               -- -20
//          | 4     |     +---+---+-----+         -- -10
//          | 40x70 o---------+ 0       |         -- 0
//  +-------+       | 6       | 70x60   |         -- 40
//  | 2     +-------+ 80x55   +------+--+-----+   -- 50
//  | 30x60 |       +---------+      | 3      |   -- 55
//  |       |                        | 55x65  |
//  +-------+               +--------+        |   -- 100
//                          | 5      +--------+   -- 115
//                          | 65x20  |
//                          +--------+            -- 120
//  |       |       |     | | |   |  |  |     |
//  -       -       0     6 7 8   1  1  1     1
//  7       4             0 0 0   2  3  5     9
//  0       0                     0  5  0     0
TEST_F(DesktopDisplayInfoTest, MAYBE_Multimon7) {
  AddDisplay(80, -10, 70, 60);
  AddDisplay(60, -50, 60, 40);
  AddDisplay(-70, 40, 30, 60);
  AddDisplay(135, 50, 55, 65);
  AddDisplay(-40, -20, 40, 70);
  AddDisplay(70, 100, 65, 20);
  AddDisplay(0, 0, 80, 55);  // Default display.

  VerifyDisplayOffset(FROM_HERE, 0, 150, 40);
  VerifyDisplayOffset(FROM_HERE, 1, 130, 0);
  VerifyDisplayOffset(FROM_HERE, 2, 0, 90);
  VerifyDisplayOffset(FROM_HERE, 3, 205, 100);
  VerifyDisplayOffset(FROM_HERE, 4, 30, 30);
  VerifyDisplayOffset(FROM_HERE, 5, 140, 150);
  VerifyDisplayOffset(FROM_HERE, 6, 70, 50);
}

}  // namespace remoting
