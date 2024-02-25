# DIP to Screen/Screen to DIP Conversion

## Overview

Converting positions and rectangles in screen physical pixels to screen DIPs
(device-independent pixels) in Chromium -- and vice versa -- is handled
differently on Windows compared to other platforms. This difference is due to
Windows' historical challenges with handling displays of DPIs higher than 96 and
 the need to deal with multiple DPI displays.

## Windows

On Windows, the process involves dealing with monitor physical coordinates and
mapping them into Chrome's DIP (Device Independent Pixels) coordinates. The key
points in this conversion process are as follows:

1. **Monitor Tree:** `DisplayInfosToScreenWinDisplays` reasons over monitors as
a tree using the primary monitor as the root. All monitors touching this root
are considered children.

2. **Connected Components:** This approach presumes that all monitors are
connected components. Windows restricts the layout of monitors to connected
components, except when DPI virtualization is happening. In the case of DPI
virtualization, scaling is relative to the (0, 0) point.

3. **Overlap Handling:** There can be cases where a scaled display may have
insufficient room to lay out its children. In these cases, a DIP point could map
to multiple screen points due to overlap. The first discovered screen point will
take precedence.

## Other Platforms

On other platforms, such as macOS and Linux, the display information received
from the OS is already adjusted for display scaling. This makes it easier to
convert between screen coordinates and screen DIPs by simply multiplying or
dividing by the current screen's display scale factor.

## How to Use

- **Windows:**
  - The conversion of screen DIPs to screen coordinates is handled by the
  `ScreenWin` class (`ui/display/win/screen_win.cc`). This class takes into
  account the monitor tree, connected components, and overlap handling when
  converting coordinates.
  - Example:
  `display::win::ScreenWin::DIPToScreenRect(GetParentHWND(), dip_bounds)` will
  convert a screen DIP rect to a screen physical one.

- **Other Platforms:**
  - You can convert physical coordinates to DIPs and vice versa by multiplying
  or dividing by the current screen's display scale factor.
