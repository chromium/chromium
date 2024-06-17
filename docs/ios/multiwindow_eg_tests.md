# Adding Multiwindow EG Tests

## Overview

This document is a simple guidance on adding Multiwindow EG tests, using a
limited set of helper functions.

## Window Number

Windows are numbered, through their `accessibilityIdentifier`, in the order they
are created. The first window will be `@"0"`, the second window will be `@"1"`,
etc. In most helper functions, the integer is used to identify windows.

Windows are not automatically renumbered so it is possible to end up with two
windows with the same number. You can use
`[ChromeEarlGrey changeWindowWithNumber:toNewNumber:]` to fix that in the
unlikely case it is needed.  Depending on the needs of the test, you can decide
on how to proceed, for example wanting to keep the left window as 0 and the
right window as 1.  See
[`[BrowserViewControllerTestCase testMultiWindowURLLoading]`](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/browser/browser_view/ui_bundled/browser_view_controller_egtest.mm;l=209)
as an example of this.

## Helpers

Multiwindow helpers are divided into two groups: window management functions and
tabs management functions, the latter being similar to their previously existing
single window counterpart versions but with an extra `inWindowWithNumber`
parameter.

The helpers all live in
[ios/chrome/test/earl_grey/chrome_earl_grey.h](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/test/earl_grey/chrome_earl_grey.h)

### Window Management

#### Window Creation

You can artificially create a new window, as if the user had done it through
the dock:

```
// Opens a new window.
- (void)openNewWindow;
```

Or by triggering any chrome function that opens one. Either way, it is strongly
recommended to call the following function to wait and verify that the action
worked (It can also be a good idea to call it before as well as after the
action):

```
// Waits for there to be |count| number of browsers within a timeout,
// or a GREYAssert is induced.
- (void)waitForForegroundWindowCount:(NSUInteger)count;
````

Two helpers allow getting the current window counts. Note that calling
`[ChromeEarlGrey waitForForegroundWindowCount:]` above is probably a better
choice than asserting on these counts.

```
// Returns the number of windows, including background and disconnected or
// archived windows.
- (NSUInteger)windowCount WARN_UNUSED_RESULT;

// Returns the number of foreground (visible on screen) windows.
- (NSUInteger)foregroundWindowCount WARN_UNUSED_RESULT;
```

#### Window destruction

You can manually destroy a window:

```
// Closes the window with given number. Note that numbering doesn't change and
// if a new window is to be added in a test, a renumbering might be needed.
- (void)closeWindowWithNumber:(int)windowNumber;
```

Or destroy all but one, leaving the test application ready for the next test.

```
// Closes all but one window, including all non-foreground windows. No-op if
// only one window presents.
- (void)closeAllExtraWindows;
```

#### Window renumbering

As discussed before, it is possible to renumber a window. Note that if more
than one window has the same number, only the first one found will be renamed.

```
// Renumbers given window with current number to new number. For example, if
// you have windows 0 and 1, close window 0, then add new window, then both
// windows would be 1. Therefore you should renumber the remaining ones
// before adding new ones.
- (void)changeWindowWithNumber:(int)windowNumber
                  toNewNumber:(int)newWindowNumber;
```

### Tab Management

All the following functions work like their non multiwindow counterparts. Note
that those non multiwindow counterparts can still be called in a multiwindow
test when only one window is visible, but their result becomes undefined if
more than one window exists.

```
// Opens a new tab in window with given number and waits for the new tab
// animation to complete within a timeout, or a GREYAssert is induced.
- (void)openNewTabInWindowWithNumber:(int)windowNumber;

// Loads |URL| in the current WebState for window with given number, with
// transition type ui::PAGE_TRANSITION_TYPED, and if waitForCompletion is YES
// waits for the loading to complete within a timeout.
// Returns nil on success, or else an NSError indicating why the operation
// failed.
- (void)loadURL:(const GURL&)URL
   inWindowWithNumber:(int)windowNumber
    waitForCompletion:(BOOL)wait;

// Loads |URL| in the current WebState for window with given number, with
// transition type ui::PAGE_TRANSITION_TYPED, and waits for the loading to
// complete within a timeout. If the condition is not met within a timeout
// returns an NSError indicating why the operation failed, otherwise nil.
- (void)loadURL:(const GURL&)URL inWindowWithNumber:(int)windowNumber;

// Waits for the page to finish loading for window with given number, within a
// timeout, or a GREYAssert is induced.
- (void)waitForPageToFinishLoadingInWindowWithNumber:(int)windowNumber;

// Returns YES if the window with given number's current WebState is loading.
- (BOOL)isLoadingInWindowWithNumber:(int)windowNumber WARN_UNUSED_RESULT;

// Waits for the current web state for window with given number, to contain
// |UTF8Text|. If the condition is not met within a timeout a GREYAssert is
// induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                  inWindowWithNumber:(int)windowNumber;

// Waits for the current web state for window with given number, to contain
// |UTF8Text|. If the condition is not met within the given |timeout| a
// GREYAssert is induced.
- (void)waitForWebStateContainingText:(const std::string&)UTF8Text
                             timeout:(NSTimeInterval)timeout
                  inWindowWithNumber:(int)windowNumber;

// Waits for there to be |count| number of non-incognito tabs within a timeout,
// or a GREYAssert is induced.
- (void)waitForMainTabCount:(NSUInteger)count
        inWindowWithNumber:(int)windowNumber;

// Waits for there to be |count| number of incognito tabs within a timeout, or a
// GREYAssert is induced.
- (void)waitForIncognitoTabCount:(NSUInteger)count
             inWindowWithNumber:(int)windowNumber;
```

## Matchers

Most existing matchers can be used when multiple windows are present by setting
a global root matcher with
`[EarlGrey setRootMatcherForSubsequentInteractions:]`.  For example, in the
following blurb, the `BackButton` is tapped in window 0, then later the
`TabGridDoneButton` is tapped in window 1:

```
 [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(1)];
 [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
     performAction:grey_tap()];
 [ChromeEarlGrey waitForWebStateContainingText:kResponse1
                            inWindowWithNumber:1];

 [EarlGrey setRootMatcherForSubsequentInteractions:WindowWithNumber(0)];
 [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridDoneButton()]
     performAction:grey_tap()];
```

If `grey_tap()` fails unexpectedly and unexplainably, see Actions section below.

Thanks to the root matcher, a limited number of matchers, require the window
number to be specified. `WindowWithNumber` is useful as a root matcher and
`MatchInWindowWithNumber` if you want to match without using a root matcher:

[ios/chrome/test/earl_grey/chrome_matchers.h](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/test/earl_grey/chrome_matchers.h)

```
// Matcher for a window with a given number.
// Window numbers are assigned at scene creation. Normally, each EGTest will
// start with exactly one window with number 0. Each time a window is created,
// it is assigned an accessibility identifier equal to the number of connected
// scenes (stored as NSString). This means typically any windows created in a
// test will have consecutive numbers.
id<GREYMatcher> WindowWithNumber(int window_number);

// Shorthand matcher for creating a matcher that ensures the given matcher
// matches elements under the given window.
id<GREYMatcher> MatchInWindowWithNumber(int window_number,
                                       id<GREYMatcher> matcher);

The settings back button is the hardest matcher to get right. Their
characteristics change based on iOS versions, on device types and on an
unexplainable situation where the label appears or not.  For this reason,
there’s a special matcher for Multiwindow:

// Returns matcher for the back button on a settings menu in given window
// number.
id<GREYMatcher> SettingsMenuBackButton(int window_number);
```

## Chrome Matchers
Some chrome matchers have a version where the window number needs to be
specified. On those, the root matcher will be set and left set on return to
allow less verbosity at call site.

```
// Makes the toolbar visible by swiping downward, if necessary. Then taps on
// the Tools menu button. At least one tab needs to be open and visible when
// calling this method.
// Sets and Leaves the root matcher to the given window with |windowNumber|.
- (void)openToolsMenuInWindowWithNumber:(int)windowNumber;

// Opens the settings menu by opening the tools menu, and then tapping the
// Settings button. There will be a GREYAssert if the tools menu is open when
// calling this method.
// Sets and Leaves the root matcher to the given window with |windowNumber|.
- (void)openSettingsMenuInWindowWithNumber:(int)windowNumber;
```

For example, the following code:

```
  [EarlGrey setRootMatcherForSubsequentInteractions:
                chrome_test_util::WindowWithNumber(windowNumber)];
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:HistoryDestinationButton()];
```

Can be reduced to:

```
  [ChromeEarlGreyUI openToolsMenuInWindowWithNumber:windowNumber];
  [ChromeEarlGreyUI tapToolsMenuButton:HistoryDestinationButton()];
```

## Actions

There are actions that cannot be done using `EarlGrey` (yet?).  One of those is
Drag and drop. But XCUI is good at that, so there are two new
client-side-triggered actions that can be used, that work across multiple
windows:

[ios/chrome/test/earl_grey/chrome_xcui_actions.h](https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/test/earl_grey/chrome_xcui_actions.h)

```
// Action (XCUI, hence local) to long press a cell item with
// |accessibility_identifier| in |window_number| and drag it to the given |edge|
// of the app screen (can trigger a new window) before dropping it. Returns YES
// on success (finding the item).
BOOL LongPressCellAndDragToEdge(NSString* accessibility_identifier,
                               GREYContentEdge edge,
                               int window_number);

// Action (XCUI, hence local) to long press a cell item  with
// |src_accessibility_identifier| in |src_window_number| and drag it to the
// given normalized offset of the cell or window with
// |dst_accessibility_identifier| in |dst_window_number| before dropping it. To
// target a window, pass nil as |dst_accessibility_identifier|. Returns YES on
// success (finding both items).
BOOL LongPressCellAndDragToOffsetOf(NSString* src_accessibility_identifier,
                                   int src_window_number,
                                   NSString* dst_accessibility_identifier,
                                   int dst_window_number,
                                   CGVector dst_normalized_offset);
```

Also there’s a bug in `EarlGrey` that makes some actions fail without a reason
on a second or third window due to a false negative visibility computation.
To palliate this for now, this XCUI action helper can be used:

```
// Action (XCUI, hence local) to tap item with |accessibility_identifier| in
// |window_number|.
BOOL TapAtOffsetOf(NSString* accessibility_identifier,
                  int window_number,
                  CGVector normalized_offset);
```

It’s hard to know when you will need it, but unexpected and unexplainable
failures in the simulator are a good clue...

If you need window resizing, the following allows you to:

```
// Action (XCUI, hence local) to resize split windows by dragging the splitter.
// This action requires two windows (|first_window_number| and
// |second_window_number|, in any order) to find where the splitter is located.
// A given |first_window_normalized_screen_size| defines the normalized size
// [0.0 - 1.0] wanted for the |first_window_number|. Returns NO if any window
// is not found or if one of them is a floating window.
// Notes: The size requested
// will be matched by the OS to the closest available multiwindow layout. This
// function works with any device orientation and with either LTR or RTL
// languages. Example of use:
//   [ChromeEarlGrey openNewWindow];
//   [ChromeEarlGrey waitForForegroundWindowCount:2];
//   chrome_test_util::DragWindowSplitterToSize(0, 1, 0.25);
// Starting with window sizes 438 and 320 pts, this will resize
// them to 320pts and 438 pts respectively.
BOOL DragWindowSplitterToSize(int first_window_number,
                             int second_window_number,
                             CGFloat first_window_normalized_screen_size);
```

## setUp/tearDown/test

In multiwindow tests, a failure to clear extra windows and root matcher at the 
end of a test, would mean a more than likely failure on the next one. To do so, 
the setUp/tearDown methods do not need any changes. ```closeAllExtraWindows``` 
and ```[EarlGrey setRootMatcherForSubsequentInteractions:nil]``` are called on 
```[ChromeTestCase tearDown]``` and ```[ChromeTestCase setUpForTestCase]```.

Tests should check if multiwindow is available on their first lines, to avoid
failing on iPhones:

```
- (void)testDragAndDropAtEdgeToCreateNewWindow {
 if (![ChromeEarlGrey areMultipleWindowsSupported])
   EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
 ...
```
