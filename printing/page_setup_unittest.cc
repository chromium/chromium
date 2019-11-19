// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/page_setup.h"

#include <stdlib.h>
#include <time.h>

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PageSetupTest, Random) {
  time_t seed = time(NULL);
  int kMax = 10;
  srand(static_cast<unsigned>(seed));

  // Margins.
  PageMargins margins;
  margins.header = rand() % kMax;
  margins.footer = rand() % kMax;
  margins.left = rand() % kMax;
  margins.top = rand() % kMax;
  margins.right = rand() % kMax;
  margins.bottom = rand() % kMax;
  int kTextHeight = rand() % kMax;

  // Page description.
  gfx::Size page_size(100 + rand() % kMax, 200 + rand() % kMax);
  gfx::Rect printable_area(rand() % kMax, rand() % kMax, 0, 0);
  printable_area.set_width(page_size.width() - (rand() % kMax) -
                           printable_area.x());
  printable_area.set_height(page_size.height() - (rand() % kMax) -
                            printable_area.y());

  // Make the calculations.
  PageSetup setup;
  setup.SetRequestedMargins(margins);
  setup.Init(page_size, printable_area, kTextHeight);

  // Calculate the effective margins.
  PageMargins effective_margins;
  effective_margins.header = std::max(margins.header, printable_area.y());
  effective_margins.left = std::max(margins.left, printable_area.x());
  effective_margins.top =
      std::max(margins.top, effective_margins.header + kTextHeight);
  effective_margins.footer =
      std::max(margins.footer, page_size.height() - printable_area.bottom());
  effective_margins.right =
      std::max(margins.right, page_size.width() - printable_area.right());
  effective_margins.bottom =
      std::max(margins.bottom, effective_margins.footer + kTextHeight);

  // Calculate the overlay area.
  gfx::Rect overlay_area(
      effective_margins.left, effective_margins.header,
      page_size.width() - effective_margins.right - effective_margins.left,
      page_size.height() - effective_margins.footer - effective_margins.header);

  // Calculate the content area.
  gfx::Rect content_area(
      overlay_area.x(), effective_margins.top, overlay_area.width(),
      page_size.height() - effective_margins.bottom - effective_margins.top);

  // Test values.
  EXPECT_EQ(page_size, setup.physical_size())
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(overlay_area, setup.overlay_area())
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(content_area, setup.content_area())
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;

  EXPECT_EQ(effective_margins.header, setup.effective_margins().header)
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(effective_margins.footer, setup.effective_margins().footer)
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(effective_margins.left, setup.effective_margins().left)
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(effective_margins.top, setup.effective_margins().top)
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(effective_margins.right, setup.effective_margins().right)
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
  EXPECT_EQ(effective_margins.bottom, setup.effective_margins().bottom)
      << seed << " " << page_size.ToString() << " " << printable_area.ToString()
      << " " << kTextHeight;
}

TEST(PageSetupTest, HardCoded) {
  // Margins.
  PageMargins margins;
  margins.header = 2;
  margins.footer = 2;
  margins.left = 4;
  margins.top = 4;
  margins.right = 4;
  margins.bottom = 4;
  int kTextHeight = 3;

  // Page description.
  gfx::Size page_size(100, 100);
  gfx::Rect printable_area(3, 3, 94, 94);

  // Make the calculations.
  PageSetup setup;
  setup.SetRequestedMargins(margins);
  setup.Init(page_size, printable_area, kTextHeight);

  // Calculate the effective margins.
  PageMargins effective_margins;
  effective_margins.header = 3;
  effective_margins.left = 4;
  effective_margins.top = 6;
  effective_margins.footer = 3;
  effective_margins.right = 4;
  effective_margins.bottom = 6;

  // Calculate the overlay area.
  gfx::Rect overlay_area(4, 3, 92, 94);

  // Calculate the content area.
  gfx::Rect content_area(4, 6, 92, 88);

  // Test values.
  EXPECT_EQ(page_size, setup.physical_size())
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(overlay_area, setup.overlay_area())
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(content_area, setup.content_area())
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;

  EXPECT_EQ(effective_margins.header, setup.effective_margins().header)
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(effective_margins.footer, setup.effective_margins().footer)
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(effective_margins.left, setup.effective_margins().left)
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(effective_margins.top, setup.effective_margins().top)
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(effective_margins.right, setup.effective_margins().right)
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
  EXPECT_EQ(effective_margins.bottom, setup.effective_margins().bottom)
      << " " << page_size.ToString() << " " << printable_area.ToString() << " "
      << kTextHeight;
}

TEST(PageSetupTest, OutOfRangeMargins) {
  PageMargins margins;
  margins.header = 0;
  margins.footer = 0;
  margins.left = -10;
  margins.top = -11;
  margins.right = -12;
  margins.bottom = -13;

  gfx::Size page_size(100, 100);
  gfx::Rect printable_area(1, 2, 96, 94);

  // Make the calculations.
  PageSetup setup;
  setup.SetRequestedMargins(margins);
  setup.Init(page_size, printable_area, 0);

  EXPECT_EQ(setup.effective_margins().left, 1);
  EXPECT_EQ(setup.effective_margins().top, 2);
  EXPECT_EQ(setup.effective_margins().right, 3);
  EXPECT_EQ(setup.effective_margins().bottom, 4);

  setup.ForceRequestedMargins(margins);
  EXPECT_EQ(setup.effective_margins().left, 0);
  EXPECT_EQ(setup.effective_margins().top, 0);
  EXPECT_EQ(setup.effective_margins().right, 0);
  EXPECT_EQ(setup.effective_margins().bottom, 0);
}

TEST(PageSetupTest, FlipOrientation) {
  // Margins.
  PageMargins margins;
  margins.header = 2;
  margins.footer = 3;
  margins.left = 4;
  margins.top = 14;
  margins.right = 6;
  margins.bottom = 7;
  int kTextHeight = 5;

  // Page description.
  gfx::Size page_size(100, 70);
  gfx::Rect printable_area(8, 9, 92, 50);

  // Make the calculations.
  PageSetup setup;
  setup.SetRequestedMargins(margins);
  setup.Init(page_size, printable_area, kTextHeight);

  gfx::Rect overlay_area(8, 9, 86, 50);
  gfx::Rect content_area(8, 14, 86, 40);

  EXPECT_EQ(page_size, setup.physical_size());
  EXPECT_EQ(overlay_area, setup.overlay_area());
  EXPECT_EQ(content_area, setup.content_area());

  EXPECT_EQ(setup.effective_margins().left, 8);
  EXPECT_EQ(setup.effective_margins().top, 14);
  EXPECT_EQ(setup.effective_margins().right, 6);
  EXPECT_EQ(setup.effective_margins().bottom, 16);

  // Flip the orientation
  setup.FlipOrientation();

  // Expected values.
  gfx::Size flipped_page_size(70, 100);
  gfx::Rect flipped_printable_area(9, 0, 50, 92);
  gfx::Rect flipped_overlay_area(9, 2, 50, 90);
  gfx::Rect flipped_content_area(9, 14, 50, 73);

  // Test values.
  EXPECT_EQ(flipped_page_size, setup.physical_size());
  EXPECT_EQ(flipped_overlay_area, setup.overlay_area());
  EXPECT_EQ(flipped_content_area, setup.content_area());
  EXPECT_EQ(flipped_printable_area, setup.printable_area());

  // Margin values are updated as per the flipped values.
  EXPECT_EQ(setup.effective_margins().left, 9);
  EXPECT_EQ(setup.effective_margins().top, 14);
  EXPECT_EQ(setup.effective_margins().right, 11);
  EXPECT_EQ(setup.effective_margins().bottom, 13);

  // Force requested margins and flip the orientation.
  setup.Init(page_size, printable_area, kTextHeight);
  setup.ForceRequestedMargins(margins);
  EXPECT_EQ(setup.effective_margins().left, 4);
  EXPECT_EQ(setup.effective_margins().top, 14);
  EXPECT_EQ(setup.effective_margins().right, 6);
  EXPECT_EQ(setup.effective_margins().bottom, 7);

  // Flip the orientation
  setup.FlipOrientation();

  // Expected values.
  gfx::Rect new_printable_area(9, 0, 50, 92);
  gfx::Rect new_overlay_area(4, 2, 60, 95);
  gfx::Rect new_content_area(4, 14, 60, 79);

  // Test values.
  EXPECT_EQ(flipped_page_size, setup.physical_size());
  EXPECT_EQ(new_overlay_area, setup.overlay_area());
  EXPECT_EQ(new_content_area, setup.content_area());
  EXPECT_EQ(new_printable_area, setup.printable_area());

  // Margins values are changed respectively.
  EXPECT_EQ(setup.effective_margins().left, 4);
  EXPECT_EQ(setup.effective_margins().top, 14);
  EXPECT_EQ(setup.effective_margins().right, 6);
  EXPECT_EQ(setup.effective_margins().bottom, 7);
}

TEST(PageSetupTest, GetSymmetricalPrintableArea) {
  gfx::Rect printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(0, 0, 560, 750));
  EXPECT_EQ(gfx::Rect(52, 42, 508, 708), printable_area);

  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(50, 60, 550, 700));
  EXPECT_EQ(gfx::Rect(50, 60, 512, 672), printable_area);

  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(-1, 60, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(50, -1, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(100, 60, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(50, 100, 520, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(400, 60, 212, 700));
  EXPECT_EQ(gfx::Rect(), printable_area);
  printable_area = PageSetup::GetSymmetricalPrintableArea(
      gfx::Size(612, 792), gfx::Rect(40, 600, 212, 192));
  EXPECT_EQ(gfx::Rect(), printable_area);
}

}  // namespace printing
