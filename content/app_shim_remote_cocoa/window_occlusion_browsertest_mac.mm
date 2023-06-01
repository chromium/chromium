// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/scoped_objc_class_swizzler.h"
#import "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#import "content/app_shim_remote_cocoa/web_contents_occlusion_checker_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"

using remote_cocoa::mojom::DraggingInfo;
using remote_cocoa::mojom::DraggingInfoPtr;
using remote_cocoa::mojom::SelectionDirection;
using content::DropData;

namespace {

const int kNeverCalled = -100;

struct FeatureState {
  bool enhanced_occlusion_detection_enabled = false;
};

struct Version {
  int32_t major;
  int32_t minor;
  bool supported;
};

}  // namespace

// An NSWindow subclass that enables programmatic setting of macOS occlusion and
// miniaturize states.
@interface WebContentsHostWindowForOcclusionTesting : NSWindow {
  BOOL _miniaturizedForTesting;
}
@property(assign, nonatomic) BOOL occludedForTesting;
@property(assign, nonatomic) BOOL modifyingChildWindowList;
@end

@implementation WebContentsHostWindowForOcclusionTesting

@synthesize occludedForTesting = _occludedForTesting;
@synthesize modifyingChildWindowList = _modifyingChildWindowList;

- (NSWindowOcclusionState)occlusionState {
  return _occludedForTesting ? 0 : NSWindowOcclusionStateVisible;
}

- (void)miniaturize:(id)sender {
  // Miniaturizing a window doesn't immediately take effect (isMiniaturized
  // returns false) so fake it with a flag and removal from window list.
  _miniaturizedForTesting = YES;
  [self orderOut:nil];
}

- (void)deminiaturize:(id)sender {
  _miniaturizedForTesting = NO;
  [self orderFront:nil];
}

- (BOOL)isMiniaturized {
  return _miniaturizedForTesting;
}

- (void)addChildWindow:(NSWindow*)childWindow
               ordered:(NSWindowOrderingMode)place {
  _modifyingChildWindowList = YES;
  [super addChildWindow:childWindow ordered:place];
  _modifyingChildWindowList = NO;
}

- (void)removeChildWindow:(NSWindow*)childWindow {
  _modifyingChildWindowList = YES;
  [super removeChildWindow:childWindow];
  _modifyingChildWindowList = NO;
}

@end

@interface WebContentsViewCocoaForOcclusionTesting : WebContentsViewCocoa
@end

@implementation WebContentsViewCocoaForOcclusionTesting

- (void)updateWebContentsVisibility:
    (remote_cocoa::mojom::Visibility)windowVisibility {
  WebContentsHostWindowForOcclusionTesting* hostWindow =
      base::mac::ObjCCast<WebContentsHostWindowForOcclusionTesting>(
          [self window]);

  EXPECT_FALSE([hostWindow modifyingChildWindowList]);

  [super updateWebContentsVisibility:windowVisibility];
}

@end

// A class that waits for invocations of the private
// -performOcclusionStateUpdates method in
// WebContentsOcclusionCheckerMac to complete.
@interface WebContentVisibilityUpdateWatcher : NSObject
@end

@implementation WebContentVisibilityUpdateWatcher

+ (std::unique_ptr<base::mac::ScopedObjCClassSwizzler>&)
    performOcclusionStateUpdatesSwizzler {
  // The swizzler needs to be generally available (i.e. not stored in an
  // instance variable) because we want to call the original
  // -performOcclusionStateUpdates from the swapped-in version
  // defined below. At the point where the swapped-in version is
  // called, the callee is an instance of WebContentsOcclusionCheckerMac,
  // not WebContentVisibilityUpdateWatcher, so it has no access to any
  // instance variables we define for WebContentVisibilityUpdateWatcher.
  // Storing the swizzler in a static makes it available to any caller.
  static base::NoDestructor<std::unique_ptr<base::mac::ScopedObjCClassSwizzler>>
      performOcclusionStateUpdatesSwizzler;

  return *performOcclusionStateUpdatesSwizzler;
}

+ (std::unique_ptr<base::mac::ScopedObjCClassSwizzler>&)
    setWebContentsOccludedSwizzler {
  static base::NoDestructor<std::unique_ptr<base::mac::ScopedObjCClassSwizzler>>
      setWebContentsOccludedSwizzler;

  return *setWebContentsOccludedSwizzler;
}

// A global place to stash the runLoop.
+ (base::RunLoop**)runLoop {
  static base::RunLoop* runLoop = nullptr;

  return &runLoop;
}

- (instancetype)init {
  self = [super init];

  // The tests should access WebContentsOcclusionCheckerMac directly, rather
  // than through NSClassFromString(). See crbug.com/1450724 .
  [WebContentVisibilityUpdateWatcher performOcclusionStateUpdatesSwizzler]
      .reset(new base::mac::ScopedObjCClassSwizzler(
          NSClassFromString(@"WebContentsOcclusionCheckerMac"),
          [WebContentVisibilityUpdateWatcher class],
          @selector(performOcclusionStateUpdates)));

  [WebContentVisibilityUpdateWatcher setWebContentsOccludedSwizzler].reset(
      new base::mac::ScopedObjCClassSwizzler(
          NSClassFromString(@"WebContentsViewCocoa"),
          [WebContentVisibilityUpdateWatcher class],
          @selector(performDelayedSetWebContentsOccluded)));

  return self;
}

- (void)dealloc {
  [WebContentVisibilityUpdateWatcher performOcclusionStateUpdatesSwizzler]
      .reset();
  [WebContentVisibilityUpdateWatcher setWebContentsOccludedSwizzler].reset();
  [super dealloc];
}

- (void)waitForOcclusionUpdate:(NSTimeInterval)delayInMilliseconds {
  // -performOcclusionStateUpdates is invoked by
  // -performSelector:afterDelay: which means it will only get called after
  // a turn of the run loop. So, we don't have to worry that it might have
  // already been called, which would block us here until the test timed out.
  base::RunLoop runLoop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, runLoop.QuitClosure(),
      base::Milliseconds(delayInMilliseconds));
  (*[WebContentVisibilityUpdateWatcher runLoop]) = &runLoop;
  runLoop.Run();
  (*[WebContentVisibilityUpdateWatcher runLoop]) = nullptr;
}

- (void)performOcclusionStateUpdates {
  // Proceed with the notification.
  [WebContentVisibilityUpdateWatcher performOcclusionStateUpdatesSwizzler]
      ->InvokeOriginal<void>(self, @selector(performOcclusionStateUpdates));

  if (*[WebContentVisibilityUpdateWatcher runLoop]) {
    (*[WebContentVisibilityUpdateWatcher runLoop])->Quit();
  }
}

- (void)performDelayedSetWebContentsOccluded {
  // Proceed with the notification.
  [WebContentVisibilityUpdateWatcher setWebContentsOccludedSwizzler]
      ->InvokeOriginal<void>(self,
                             @selector(performDelayedSetWebContentsOccluded));

  if (*[WebContentVisibilityUpdateWatcher runLoop]) {
    (*[WebContentVisibilityUpdateWatcher runLoop])->Quit();
  }
}

@end

// A class that counts invocations of the public
// -scheduleOcclusionStateUpdates method in WebContentsOcclusionCheckerMac.
@interface WebContentVisibilityUpdateCounter : NSObject
@end

@implementation WebContentVisibilityUpdateCounter

+ (std::unique_ptr<base::mac::ScopedObjCClassSwizzler>&)swizzler {
  static base::NoDestructor<std::unique_ptr<base::mac::ScopedObjCClassSwizzler>>
      swizzler;

  return *swizzler;
}

+ (NSInteger&)methodInvocationCount {
  static NSInteger invocationCount = 0;

  return invocationCount;
}

+ (BOOL)methodNeverCalled {
  return
      [WebContentVisibilityUpdateCounter methodInvocationCount] == kNeverCalled;
}

- (instancetype)init {
  self = [super init];

  // Set up the swizzling.
  [WebContentVisibilityUpdateCounter swizzler].reset(
      new base::mac::ScopedObjCClassSwizzler(
          NSClassFromString(@"WebContentsOcclusionCheckerMac"),
          [WebContentVisibilityUpdateCounter class],
          @selector(scheduleOcclusionStateUpdates)));

  [WebContentVisibilityUpdateCounter methodInvocationCount] = kNeverCalled;

  return self;
}

- (void)dealloc {
  [WebContentVisibilityUpdateCounter methodInvocationCount] = 0;
  [super dealloc];
}

- (void)scheduleOcclusionStateUpdates {
  // Proceed with the scheduling.
  [WebContentVisibilityUpdateCounter swizzler]->InvokeOriginal<void>(
      self, @selector(scheduleOcclusionStateUpdates));

  NSInteger count = [WebContentVisibilityUpdateCounter methodInvocationCount];
  if (count < 0) {
    count = 0;
  }
  [WebContentVisibilityUpdateCounter methodInvocationCount] = count + 1;
}

@end

namespace content {

// A stub class for WebContentsNSViewHost.
class WebContentsNSViewHostStub
    : public remote_cocoa::mojom::WebContentsNSViewHost {
 public:
  WebContentsNSViewHostStub() {}

  void OnMouseEvent(bool motion, bool exited) override {}

  void OnBecameFirstResponder(SelectionDirection direction) override {}

  void OnWindowVisibilityChanged(
      remote_cocoa::mojom::Visibility visibility) override {
    _visibility = visibility;
  }

  remote_cocoa::mojom::Visibility WebContentsVisibility() {
    return _visibility;
  }

  void SetDropData(const ::content::DropData& drop_data) override {}

  bool DraggingEntered(DraggingInfoPtr dragging_info,
                       uint32_t* out_result) override {
    return false;
  }

  void DraggingEntered(DraggingInfoPtr dragging_info,
                       DraggingEnteredCallback callback) override {}

  void DraggingExited() override {}

  void DraggingUpdated(DraggingInfoPtr dragging_info,
                       DraggingUpdatedCallback callback) override {}

  bool PerformDragOperation(DraggingInfoPtr dragging_info,
                            bool* out_result) override {
    return false;
  }

  void PerformDragOperation(DraggingInfoPtr dragging_info,
                            PerformDragOperationCallback callback) override {}

  bool DragPromisedFileTo(const ::base::FilePath& file_path,
                          const ::content::DropData& drop_data,
                          const ::GURL& download_url,
                          ::base::FilePath* out_file_path) override {
    return false;
  }

  void DragPromisedFileTo(const ::base::FilePath& file_path,
                          const ::content::DropData& drop_data,
                          const ::GURL& download_url,
                          DragPromisedFileToCallback callback) override {}

  void EndDrag(uint32_t drag_operation,
               const ::gfx::PointF& local_point,
               const ::gfx::PointF& screen_point) override {}

 private:
  remote_cocoa::mojom::Visibility _visibility;
};

// Sets up occlusion tests.
class WindowOcclusionBrowserTestMac
    : public ::testing::WithParamInterface<FeatureState>,
      public ContentBrowserTest {
 public:
  WindowOcclusionBrowserTestMac() {
    if (GetParam().enhanced_occlusion_detection_enabled) {
      base::FieldTrialParams params;
      params["EnhancedWindowOcclusionDetection"] = "true";
      _features.InitAndEnableFeatureWithParameters(
          features::kMacWebContentsOcclusion, params);
    } else {
      _features.InitAndDisableFeature(features::kMacWebContentsOcclusion);
    }
  }

  void SetUp() override {
    if (![NSClassFromString(@"WebContentsOcclusionCheckerMac")
            manualOcclusionDetectionSupportedForCurrentMacOSVersion]) {
      GTEST_SKIP()
          << "Manual window occlusion detection is broken on macOS 13.0-13.2.";
    }
    ContentBrowserTest::SetUp();
  }

  ~WindowOcclusionBrowserTestMac() {
    [NSClassFromString(@"WebContentsOcclusionCheckerMac")
        resetSharedInstanceForTesting];
  }

  bool WebContentsAwaitingUpdates() {
    NSMutableArray<WebContentsViewCocoa*>* allWebContentsViewCocoa =
        [NSMutableArray array];

    [allWebContentsViewCocoa
        addObjectsFromArray:[window_a webContentsViewCocoa]];
    [allWebContentsViewCocoa
        addObjectsFromArray:[window_b webContentsViewCocoa]];

    // Add these explicitly, in case they've been removed from their host
    // windows.
    if (window_a_web_contents_view_cocoa &&
        ![allWebContentsViewCocoa
            containsObject:window_a_web_contents_view_cocoa])
      [allWebContentsViewCocoa addObject:window_a_web_contents_view_cocoa];

    if (window_b_web_contents_view_cocoa &&
        ![allWebContentsViewCocoa
            containsObject:window_b_web_contents_view_cocoa])
      [allWebContentsViewCocoa addObject:window_b_web_contents_view_cocoa];

    for (WebContentsViewCocoa* webContentsViewCocoa in
             allWebContentsViewCocoa) {
      if ([webContentsViewCocoa
              willSetWebContentsOccludedAfterDelayForTesting]) {
        return true;
      }
    }

    return false;
  }

  void WaitForOcclusionUpdate() {
    if (!base::FeatureList::IsEnabled(features::kMacWebContentsOcclusion))
      return;

    while ([[NSClassFromString(@"WebContentsOcclusionCheckerMac")
               sharedInstance] occlusionStateUpdatesAreScheduledForTesting] ||
           WebContentsAwaitingUpdates()) {
      base::scoped_nsobject<WebContentVisibilityUpdateWatcher> watcher(
          [[WebContentVisibilityUpdateWatcher alloc] init]);
      [watcher waitForOcclusionUpdate:1200];
    }
  }

  static WebContentsViewCocoaForOcclusionTesting* WebContentsInWindow(
      NSRect contentRect,
      NSWindowStyleMask styleMask = NSWindowStyleMaskClosable) {
    WebContentsHostWindowForOcclusionTesting* window =
        [[[WebContentsHostWindowForOcclusionTesting alloc]
            initWithContentRect:contentRect
                      styleMask:styleMask
                        backing:NSBackingStoreBuffered
                          defer:YES] autorelease];
    NSRect window_frame = [NSWindow frameRectForContentRect:contentRect
                                                  styleMask:styleMask];
    window_frame.origin = NSMakePoint(20.0, 200.0);
    [window setFrame:window_frame display:NO];
    [window setReleasedWhenClosed:NO];

    const NSRect kWebContentsFrame = NSMakeRect(0.0, 0.0, 10.0, 10.0);
    WebContentsViewCocoaForOcclusionTesting* web_contents_view =
        [[[WebContentsViewCocoaForOcclusionTesting alloc]
            initWithFrame:kWebContentsFrame] autorelease];
    [[window contentView] addSubview:web_contents_view];

    return web_contents_view;
  }

  // Creates |window_a| with a visible (i.e. unoccluded) WebContentsViewCocoa.
  void InitWindowA() {
    const NSRect kWindowAContentRect = NSMakeRect(0.0, 0.0, 80.0, 60.0);
    window_a_web_contents_view_cocoa.reset(
        [WebContentsInWindow(kWindowAContentRect) retain]);
    window_a.reset(
        base::mac::ObjCCast<WebContentsHostWindowForOcclusionTesting>(
            [[window_a_web_contents_view_cocoa window] retain]));
    [window_a setTitle:@"window_a"];

    // Set up a fake host so we can check the occlusion status.
    [window_a_web_contents_view_cocoa setHost:&_host_a];

    // Bring the browser window onscreen.
    OrderWindowFront(window_a);

    // Init visibility state.
    SetWindowAWebContentsVisibility(remote_cocoa::mojom::Visibility::kVisible);
  }

  void InitWindowB(NSRect window_frame = NSZeroRect) {
    const NSRect kWindowBContentRect = NSMakeRect(0.0, 0.0, 40.0, 40.0);
    window_b_web_contents_view_cocoa.reset(
        [WebContentsInWindow(kWindowBContentRect) retain]);
    window_b.reset(
        base::mac::ObjCCast<WebContentsHostWindowForOcclusionTesting>(
            [[window_b_web_contents_view_cocoa window] retain]));
    [window_b setTitle:@"window_b"];

    if (NSIsEmptyRect(window_frame)) {
      window_frame.size =
          [NSWindow frameRectForContentRect:kWindowBContentRect
                                  styleMask:[window_b styleMask]]
              .size;
    }
    [window_b setFrame:window_frame display:NO];

    OrderWindowFront(window_b);
  }

  void OrderWindowFront(NSWindow* window) {
    base::scoped_nsobject<WebContentVisibilityUpdateCounter> watcher;

    if (!kEnhancedWindowOcclusionDetection.Get()) {
      watcher.reset([[WebContentVisibilityUpdateCounter alloc] init]);
    }

    [window orderWindow:NSWindowAbove relativeTo:0];
    ASSERT_TRUE([window isVisible]);

    if (kEnhancedWindowOcclusionDetection.Get()) {
      WaitForOcclusionUpdate();
    }
  }

  void OrderWindowOut(NSWindow* window) {
    [window orderWindow:NSWindowOut relativeTo:0];
    ASSERT_FALSE([window isVisible]);

    WaitForOcclusionUpdate();
  }

  void CloseWindow(NSWindow* window) {
    [window close];
    ASSERT_FALSE([window isVisible]);

    WaitForOcclusionUpdate();
  }

  void MiniaturizeWindow(NSWindow* window) {
    [window miniaturize:nil];

    WaitForOcclusionUpdate();
  }

  void DeminiaturizeWindow(NSWindow* window) {
    [window deminiaturize:nil];

    WaitForOcclusionUpdate();
  }

  void AddSubviewOfView(NSView* subview, NSView* view) {
    [view addSubview:subview];

    WaitForOcclusionUpdate();
  }

  void SetViewHidden(NSView* view, BOOL hidden) {
    [view setHidden:hidden];

    WaitForOcclusionUpdate();
  }

  void RemoveViewFromSuperview(NSView* view) {
    [view removeFromSuperview];

    WaitForOcclusionUpdate();
  }

  void PostNotification(NSString* notification_name, id object = nil) {
    [[NSNotificationCenter defaultCenter] postNotificationName:notification_name
                                                        object:object
                                                      userInfo:nil];
    WaitForOcclusionUpdate();
  }

  void PostWorkspaceNotification(NSString* notification_name) {
    ASSERT_TRUE([[NSWorkspace sharedWorkspace] notificationCenter]);
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        postNotificationName:notification_name
                      object:nil
                    userInfo:nil];
    WaitForOcclusionUpdate();
  }

  remote_cocoa::mojom::Visibility WindowAWebContentsVisibility() {
    return _host_a.WebContentsVisibility();
  }

  void SetWindowAWebContentsVisibility(
      remote_cocoa::mojom::Visibility visibility) {
    _host_a.OnWindowVisibilityChanged(visibility);
  }

  void TearDownInProcessBrowserTestFixture() override {
    [window_a_web_contents_view_cocoa setHost:nullptr];
  }

  base::scoped_nsobject<WebContentsHostWindowForOcclusionTesting> window_a;
  base::scoped_nsobject<WebContentsViewCocoa> window_a_web_contents_view_cocoa;
  base::scoped_nsobject<WebContentsHostWindowForOcclusionTesting> window_b;
  base::scoped_nsobject<WebContentsViewCocoa> window_b_web_contents_view_cocoa;

 private:
  base::test::ScopedFeatureList _features;
  WebContentsNSViewHostStub _host_a;
};

using WindowOcclusionBrowserTestMacWithoutOcclusionFeature =
    WindowOcclusionBrowserTestMac;
using WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature =
    WindowOcclusionBrowserTestMac;

// Tests that should only work without the occlusion detection feature.
INSTANTIATE_TEST_SUITE_P(NoFeature,
                         WindowOcclusionBrowserTestMacWithoutOcclusionFeature,
                         ::testing::Values(FeatureState{
                             .enhanced_occlusion_detection_enabled = false}));

// Tests that should work with or without the occlusion detection feature.
INSTANTIATE_TEST_SUITE_P(
    Common,
    WindowOcclusionBrowserTestMac,
    ::testing::Values(
        FeatureState{.enhanced_occlusion_detection_enabled = false},
        FeatureState{.enhanced_occlusion_detection_enabled = true}));

// Tests that require enhanced window occlusion detection.
INSTANTIATE_TEST_SUITE_P(
    EnhancedWindowOcclusionDetection,
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ::testing::Values(FeatureState{
        .enhanced_occlusion_detection_enabled = true}));

// Tests that we correctly disallow unsupported macOS versions.
IN_PROC_BROWSER_TEST_P(WindowOcclusionBrowserTestMac, MacOSVersionChecking) {
  Class WebContentsOcclusionCheckerMac =
      NSClassFromString(@"WebContentsOcclusionCheckerMac");
  std::vector<Version> versions = {
      {11, 0, true},  {12, 0, true},  {12, 9, true}, {13, 0, false},
      {13, 1, false}, {13, 2, false}, {13, 3, true}, {14, 0, true}};

  for (const auto& version : versions) {
    bool supported = [WebContentsOcclusionCheckerMac manualOcclusionDetectionSupportedForVersion:version.major
                                                                                                :version.minor];
    EXPECT_EQ(supported, version.supported);
  }
}

// Tests that enhanced occlusion detection isn't triggered if the feature's
// not enabled.
IN_PROC_BROWSER_TEST_P(WindowOcclusionBrowserTestMacWithoutOcclusionFeature,
                       ManualOcclusionDetectionDisabled) {
  InitWindowA();

  // Create a second window and place it exactly over window_a. The window
  // should still be considered visible.
  InitWindowB([window_a frame]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Test that display sleep and app hide detection don't work if the feature's
// not enabled.
IN_PROC_BROWSER_TEST_P(WindowOcclusionBrowserTestMacWithoutOcclusionFeature,
                       OcclusionDetectionOnDisplaySleepDisabled) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake a display sleep notification.
  ASSERT_TRUE([[NSWorkspace sharedWorkspace] notificationCenter]);
  base::scoped_nsobject<WebContentVisibilityUpdateCounter> watcher(
      [[WebContentVisibilityUpdateCounter alloc] init]);

  [[[NSWorkspace sharedWorkspace] notificationCenter]
      postNotificationName:NSWorkspaceScreensDidSleepNotification
                    object:nil
                  userInfo:nil];

  EXPECT_TRUE([WebContentVisibilityUpdateCounter methodNeverCalled]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Test that we properly handle occlusion notifications from macOS.
IN_PROC_BROWSER_TEST_P(WindowOcclusionBrowserTestMac,
                       MacOSOcclusionNotifications) {
  InitWindowA();

  [window_a setOccludedForTesting:YES];
  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  [window_a setOccludedForTesting:NO];
  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetection) {
  InitWindowA();

  // Create a second window and place it exactly over window_a. Unlike macOS,
  // our manual occlusion detection will determine window_a is occluded.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Move window_b slightly in different directions and check the occlusion
  // state of window_a's web contents.
  const NSSize window_offsets[] = {
      {1.0, 0.0}, {-1.0, 0.0}, {0.0, 1.0}, {0.0, -1.0}};
  NSRect window_b_frame = [window_b frame];
  for (size_t i = 0; i < std::size(window_offsets); i++) {
    // Move window b so that it no longer completely covers
    // window_a's webcontents.
    NSRect offset_window_frame = NSOffsetRect(
        window_b_frame, window_offsets[i].width, window_offsets[i].height);
    [window_b setFrame:offset_window_frame display:YES];

    WaitForOcclusionUpdate();
    EXPECT_EQ(WindowAWebContentsVisibility(),
              remote_cocoa::mojom::Visibility::kVisible);

    // Move it back.
    [window_b setFrame:window_b_frame display:YES];

    WaitForOcclusionUpdate();
    EXPECT_EQ(WindowAWebContentsVisibility(),
              remote_cocoa::mojom::Visibility::kOccluded);
  }
}

// Checks manual occlusion detection as windows change display order.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnWindowOrderChange) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  OrderWindowFront(window_a);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  OrderWindowFront(window_b);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
}

// Checks that window_a, occluded by window_b, transitions to kVisible while the
// user resizes window_b.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnWindowLiveResize) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Fake the start of a live resize. window_a's web contents should
  // become kVisible because resizing window_b may expose whatever's
  // behind it.
  PostNotification(NSWindowWillStartLiveResizeNotification, window_b);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake the resize end, which should return window_a to kOccluded because
  // it's still completely covered by window_b.
  PostNotification(NSWindowDidEndLiveResizeNotification, window_b);
  WaitForOcclusionUpdate();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
}

// Checks that window_a, occluded by window_b, transitions to kVisible when
// window_b is set to close.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnWindowClose) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Close window b.
  CloseWindow(window_b);

  // window_a's web contents should be kVisible, so that it's properly
  // updated when window_b goes offscreen.
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that window_a, occluded by window_b and window_c, remains kOccluded
// when window_b is set to close.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ManualOcclusionDetectionOnMiddleWindowClose) {
  InitWindowA();

  // Size and position the second window so that it exactly covers the
  // first.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Create a window_c on top of them both.
  const NSRect kWindowCContentRect = NSMakeRect(0.0, 0.0, 80.0, 60.0);
  base::scoped_nsobject<NSWindow> window_c(
      [[WebContentsInWindow(kWindowCContentRect) window] retain]);
  [window_c setTitle:@"window_c"];

  // Configure it for the test.
  [window_c setFrame:[window_a frame] display:NO];
  OrderWindowFront(window_c);

  // Close window_b.
  CloseWindow(window_b);
  WaitForOcclusionUpdate();

  // window_a's web contents should remain kOccluded because of window_c.
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
}

// Checks that web contents are marked kHidden on display sleep.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionOnDisplaySleep) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake a display sleep notification.
  PostWorkspaceNotification(NSWorkspaceScreensDidSleepNotification);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Fake a display wake notification.
  PostWorkspaceNotification(NSWorkspaceScreensDidWakeNotification);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that occlusion updates are ignored in between fullscreen transition
// notifications.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMac,
    //                       WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    IgnoreOcclusionUpdatesBetweenWindowFullscreenTransitionNotifications) {
  InitWindowA();

  [window_a setOccluded:NO];
  [window_a setOccludedForTesting:NO];

  // Fake a fullscreen transition notification.
  PostNotification(NSWindowWillEnterFullScreenNotification, window_a);

  // An occlusion change should have no effect while in transition.
  [window_a setOccludedForTesting:YES];
  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // End the transition.
  PostNotification(NSWindowDidExitFullScreenNotification, window_a);

  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);

  WaitForOcclusionUpdate();

  // Check the web contents visibility state rather than the window's occlusion
  // state because -isOccluded, added by a category, does not ever return YES
  // unless manual window occlusion is enabled.
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Reset.
  [window_a setOccluded:NO];
  [window_a setOccludedForTesting:NO];
  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);
  WaitForOcclusionUpdate();
  ASSERT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Fake the exit transition start.
  PostNotification(NSWindowWillExitFullScreenNotification, window_a);

  [window_a setOccludedForTesting:YES];
  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // End the transition.
  PostNotification(NSWindowDidExitFullScreenNotification, window_a);

  PostNotification(NSWindowDidChangeOcclusionStateNotification, window_a);

  WaitForOcclusionUpdate();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
  //  EXPECT_TRUE([window_a isOccluded]);
}

// Tests that each web contents in a window receives an updated occlusion
// state updated.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionForMultipleWebContents) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Create a second web contents.
  const NSRect kWebContentsBFrame = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  WebContentsViewCocoa* web_contents_b =
      [[WebContentsViewCocoaForOcclusionTesting alloc]
          initWithFrame:kWebContentsBFrame];
  [[window_a contentView] addSubview:web_contents_b];
  WebContentsNSViewHostStub host_2;
  [web_contents_b setHost:&host_2];
  host_2.OnWindowVisibilityChanged(remote_cocoa::mojom::Visibility::kVisible);

  const NSRect kWebContentsCFrame = NSMakeRect(0.0, 20.0, 10.0, 10.0);
  WebContentsViewCocoa* web_contents_c =
      [[WebContentsViewCocoaForOcclusionTesting alloc]
          initWithFrame:kWebContentsCFrame];
  [[window_a contentView] addSubview:web_contents_c];
  WebContentsNSViewHostStub host_3;
  [web_contents_c setHost:&host_3];
  host_3.OnWindowVisibilityChanged(remote_cocoa::mojom::Visibility::kVisible);

  // Add window_b to occlude window_a and its web contentses.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
  EXPECT_EQ(host_2.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);
  EXPECT_EQ(host_3.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Close window b, which should expose the web contentses.
  CloseWindow(window_b);

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
  EXPECT_EQ(host_2.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
  EXPECT_EQ(host_3.WebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  [web_contents_b setHost:nullptr];
  [web_contents_c setHost:nullptr];
}

// Checks that web contentses are marked kHidden on WebContentsViewCocoa hide.
IN_PROC_BROWSER_TEST_P(WindowOcclusionBrowserTestMac,
                       OcclusionDetectionOnWebContentsViewCocoaHide) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  SetViewHidden(window_a_web_contents_view_cocoa, YES);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  SetViewHidden(window_a_web_contents_view_cocoa, NO);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Hiding the superview should have the same effect.
  SetViewHidden([window_a_web_contents_view_cocoa superview], YES);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  SetViewHidden([window_a_web_contents_view_cocoa superview], NO);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that web contentses are marked kHidden on WebContentsViewCocoa removal
// from the view hierarchy.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMac,
    OcclusionDetectionOnWebContentsViewCocoaRemoveFromSuperview) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  RemoveViewFromSuperview(window_a_web_contents_view_cocoa);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  // Adding it back should make it visible.
  AddSubviewOfView(window_a_web_contents_view_cocoa, [window_a contentView]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  // Try the same with its superview.
  const NSRect kTmpViewFrame = NSMakeRect(0.0, 0.0, 10.0, 10.0);
  base::scoped_nsobject<NSView> tmpView(
      [[NSView alloc] initWithFrame:kTmpViewFrame]);
  [[window_a contentView] addSubview:tmpView];
  AddSubviewOfView(tmpView, [window_a contentView]);
  RemoveViewFromSuperview(window_a_web_contents_view_cocoa);
  AddSubviewOfView(window_a_web_contents_view_cocoa, tmpView);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  RemoveViewFromSuperview(tmpView);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kHidden);

  AddSubviewOfView(tmpView, [window_a contentView]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Checks that web contentses are marked kHidden on window miniaturize.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    OcclusionDetectionOnWindowMiniaturize) {
  InitWindowA();

  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  MiniaturizeWindow(window_a);

  EXPECT_TRUE([window_a isMiniaturized]);
  EXPECT_NE(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);

  DeminiaturizeWindow(window_a);

  EXPECT_FALSE([window_a isMiniaturized]);
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

// Tests that occlusion updates only occur after a child window has been
// added to or removed from a parent. In Chrome, some webcontents visibility
// watchers add child windows (bubbles) when visibility changes. We want to
// avoid the situation where a browser component adds a child window,
// triggering a visility update, which causes a visibility watcher to add
// a second child window (while we're still inside AppKit code adding the
// first).
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    ChildWindowListMutationDuringManualOcclusionDetection) {
  InitWindowA();

  const NSRect kContentRect = NSMakeRect(0.0, 0.0, 20.0, 20.0);
  WebContentsViewCocoaForOcclusionTesting* child_window_web_contents =
      WindowOcclusionBrowserTestMac::WebContentsInWindow(
          kContentRect, NSWindowStyleMaskBorderless);

  // Clear out any pending occlusion updates from the window creation.
  WaitForOcclusionUpdate();

  // Add the window with the webcontents as a child. The child window coming
  // onscreen should not trigger a visibility update (at least not from us).
  // A check inside the webcontents will also ensure no updates occur while
  // the window modifies its child window list.
  [window_a addChildWindow:[child_window_web_contents window]
                   ordered:NSWindowAbove];

  EXPECT_FALSE([[NSClassFromString(@"WebContentsOcclusionCheckerMac")
      sharedInstance] occlusionStateUpdatesAreScheduledForTesting]);

  // Modify the child window list by removing a child window.
  [window_a removeChildWindow:[child_window_web_contents window]];

  EXPECT_FALSE([[NSClassFromString(@"WebContentsOcclusionCheckerMac")
      sharedInstance] occlusionStateUpdatesAreScheduledForTesting]);
}

// Tests that when a window becomes a child, if the occlusion system
// previously marked it occluded, the window transitions to visible.
IN_PROC_BROWSER_TEST_P(
    WindowOcclusionBrowserTestMacWithOcclusionDetectionFeature,
    WindowMadeChildForcedVisible) {
  InitWindowA();

  // Create a second window that occludes window_a.
  InitWindowB([window_a frame]);
  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kOccluded);

  // Make window_a a child of window_b. The occlusion system ignores
  // child windows, so ensure window_a's occlusion state changes back
  // to visible.
  [window_b addChildWindow:window_a ordered:NSWindowAbove];

  WaitForOcclusionUpdate();
  EXPECT_EQ(WindowAWebContentsVisibility(),
            remote_cocoa::mojom::Visibility::kVisible);
}

}  // namespace content
