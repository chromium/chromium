// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"

#include <AppKit/AppKit.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#import "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#import "content/app_shim_remote_cocoa/web_contents_occlusion_checker_mac.h"
#import "content/app_shim_remote_cocoa/web_drag_source_mac.h"
#import "content/browser/web_contents/web_contents_view_mac.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"

using content::DropData;
using features::kMacWebContentsOcclusion;
using remote_cocoa::mojom::DraggingInfo;
using remote_cocoa::mojom::SelectionDirection;

namespace {
// Time to delay clearing the pasteboard for after a drag ends. This is
// required because Safari requests data from multiple processes, and clearing
// the pasteboard after the first access results in unreliable drag operations
// (http://crbug.com/1227001).
const int64_t kPasteboardClearDelay = 0.5 * NSEC_PER_SEC;
}

namespace remote_cocoa {

// DroppedScreenShotCopierMac is a utility to copy screenshots to a usable
// directory for PWAs. When screenshots are taken and dragged directly on to an
// application, the resulting file can only be opened by the application that it
// is passed to. For PWAs, this means that the file dragged to the PWA is not
// accessible by the browser, resulting in a failure to open the file. This
// class works around that problem by copying such screenshot files.
// https://crbug.com/1148078
class DroppedScreenShotCopierMac {
 public:
  DroppedScreenShotCopierMac() = default;
  ~DroppedScreenShotCopierMac() {
    if (temp_dir_) {
      base::ScopedAllowBlocking allow_io;
      temp_dir_.reset();
    }
  }

  // Examine all entries in `drop_data.filenames`. If any of them look like a
  // screenshot file, copy the file to a temporary directory. This temporary
  // directory (and its contents) will be kept alive until `this` is destroyed.
  void CopyScreenShotsInDropData(content::DropData& drop_data) {
    for (auto& file_info : drop_data.filenames) {
      if (IsPathScreenShot(file_info.path)) {
        base::ScopedAllowBlocking allow_io;
        if (!temp_dir_) {
          auto new_temp_dir = std::make_unique<base::ScopedTempDir>();
          if (!new_temp_dir->CreateUniqueTempDir())
            return;
          temp_dir_ = std::move(new_temp_dir);
        }
        base::FilePath copy_path =
            temp_dir_->GetPath().Append(file_info.path.BaseName());
        if (base::CopyFile(file_info.path, copy_path))
          file_info.path = copy_path;
      }
    }
  }

 private:
  bool IsPathScreenShot(const base::FilePath& path) const {
    const std::string& value = path.value();
    size_t found_var = value.find("/var");
    if (found_var != 0)
      return false;
    size_t found_screencaptureui = value.find("screencaptureui");
    if (found_screencaptureui == std::string::npos)
      return false;
    return true;
  }

  std::unique_ptr<base::ScopedTempDir> temp_dir_;
};

}  // namespace remote_cocoa

// Ensure that the ui::DragDropTypes::DragOperation enum values stay in sync
// with NSDragOperation constants, since the code below uses
// NSDragOperationToDragOperation to filter invalid values.
#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "enum mismatch: " #a)
STATIC_ASSERT_ENUM(NSDragOperationNone, ui::DragDropTypes::DRAG_NONE);
STATIC_ASSERT_ENUM(NSDragOperationCopy, ui::DragDropTypes::DRAG_COPY);
STATIC_ASSERT_ENUM(NSDragOperationLink, ui::DragDropTypes::DRAG_LINK);
STATIC_ASSERT_ENUM(NSDragOperationMove, ui::DragDropTypes::DRAG_MOVE);

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewCocoa

@implementation WebContentsViewCocoa {
  // TODO(https://crbug.com/883031): Remove this when kMacWebContentsOcclusion
  // is enabled by default.
  BOOL _inFullScreenTransition;
  BOOL _willSetWebContentsOccludedAfterDelay;
}

+ (void)initialize {
  if (base::FeatureList::IsEnabled(kMacWebContentsOcclusion)) {
    // Create the WebContentsOcclusionCheckerMac shared instance.
    [WebContentsOcclusionCheckerMac sharedInstance];
  }
}

- (instancetype)initWithViewsHostableView:(ui::ViewsHostableView*)v {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    _viewsHostableView = v;
    [self registerDragTypes];

    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(viewDidBecomeFirstResponder:)
               name:kViewDidBecomeFirstResponder
             object:nil];
  }
  return self;
}

- (void)dealloc {
  // This probably isn't strictly necessary, but can't hurt.
  [self unregisterDraggedTypes];

  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self cancelDelayedSetWebContentsOccluded];

  [super dealloc];
}

- (void)enableDroppedScreenShotCopier {
  DCHECK(!_droppedScreenShotCopier);
  _droppedScreenShotCopier =
      std::make_unique<remote_cocoa::DroppedScreenShotCopierMac>();
}

- (void)populateDraggingInfo:(DraggingInfo*)info
          fromNSDraggingInfo:(id<NSDraggingInfo>)nsInfo {
  NSPoint windowPoint = [nsInfo draggingLocation];

  NSPoint viewPoint = [self convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [self frame];
  info->location_in_view =
      gfx::PointF(viewPoint.x, viewFrame.size.height - viewPoint.y);

  NSPoint screenPoint =
      ui::ConvertPointFromWindowToScreen([self window], windowPoint);
  NSScreen* screen = [[self window] screen];
  NSRect screenFrame = [screen frame];
  info->location_in_screen =
      gfx::PointF(screenPoint.x, screenFrame.size.height - screenPoint.y);

  NSPasteboard* pboard = [nsInfo draggingPasteboard];
  NSArray<NSString*>* urls;
  NSArray<NSString*>* titles;
  if (ui::ClipboardUtil::URLsAndTitlesFromPasteboard(
          pboard, /*include_files=*/true, &urls, &titles)) {
    info->url = GURL(base::SysNSStringToUTF8(urls.firstObject));
  }
  info->operation_mask = ui::DragDropTypes::NSDragOperationToDragOperation(
      [nsInfo draggingSourceOperationMask]);
}

- (BOOL)allowsVibrancy {
  // Returning YES will allow rendering this view with vibrancy effect if it is
  // incorporated into a view hierarchy that uses vibrancy, it will have no
  // effect otherwise.
  // For details see Apple documentation on NSView and NSVisualEffectView.
  return YES;
}

// Registers for the view for the appropriate drag types.
- (void)registerDragTypes {
  [self registerForDraggedTypes:@[
    NSPasteboardTypeFileURL, NSPasteboardTypeHTML, NSPasteboardTypeRTF,
    NSPasteboardTypeString, NSPasteboardTypeURL,
    ui::kUTTypeChromiumInitiatedDrag, ui::kUTTypeChromiumWebCustomData,
    ui::kUTTypeWebKitWebURLsWithTitles
  ]];
}

- (void)mouseEvent:(NSEvent*)theEvent {
  if (!_host)
    return;
  _host->OnMouseEvent([theEvent type] == NSEventTypeMouseMoved,
                      [theEvent type] == NSEventTypeMouseExited);
}

- (void)setMouseDownCanMoveWindow:(BOOL)canMove {
  _mouseDownCanMoveWindow = canMove;
}

- (BOOL)mouseDownCanMoveWindow {
  // This is needed to prevent mouseDowns from moving the window
  // around. The default implementation returns YES only for opaque
  // views. WebContentsViewCocoa does not draw itself in any way, but
  // its subviews do paint their entire frames. Returning NO here
  // saves us the effort of overriding this method in every possible
  // subview.
  return _mouseDownCanMoveWindow;
}

- (void)startDragWithDropData:(const DropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset {
  if (!_host)
    return;

  NSPasteboard* pasteboard =
      [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];
  [pasteboard clearContents];

  _dragSource.reset([[WebDragSource alloc] initWithHost:_host
                                                   view:self
                                               dropData:&dropData
                                                  image:image
                                                 offset:offset
                                             pasteboard:pasteboard
                                      dragOperationMask:operationMask]);
  [_dragSource startDrag];
}

// NSDraggingSource methods

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  if (_dragSource)
    return [_dragSource draggingSourceOperationMaskForLocal:isLocal];
  // No web drag source - this is the case for dragging a file from the
  // downloads manager. Default to copy operation. Note: It is desirable to
  // allow the user to either move or copy, but this requires additional
  // plumbing to update the download item's path once its moved.
  return NSDragOperationCopy;
}

// Called when a drag initiated in our view ends.
- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)screenPoint
           operation:(NSDragOperation)operation {
  [_dragSource
      endDragAt:screenPoint
      operation:ui::DragDropTypes::NSDragOperationToDragOperation(operation)];

  WebDragSource* currentDragSource = _dragSource.get();

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)kPasteboardClearDelay),
      dispatch_get_main_queue(), ^{
        if (_dragSource.get() == currentDragSource) {
          // Clear the drag pasteboard. Even though this is called in dealloc,
          // we need an explicit call because NSPasteboard can retain the drag
          // source.
          [_dragSource clearPasteboard];
          _dragSource.reset();
        }
      });
}

// Called when a drag initiated in our view moves.
- (void)draggedImage:(NSImage*)draggedImage movedTo:(NSPoint)screenPoint {
}

// Called when a file drag is dropped and the promised files need to be written.
- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDest {
  if (![dropDest isFileURL])
    return nil;

  NSString* fileName = [_dragSource dragPromisedFileTo:[dropDest path]];
  if (!fileName)
    return nil;

  return @[ fileName ];
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  if (!_host)
    return NSDragOperationNone;

  // Fill out a DropData from pasteboard.
  DropData dropData;
  content::PopulateDropDataFromPasteboard(&dropData,
                                          [sender draggingPasteboard]);

  // Work around screen shot drag-drop permission bugs.
  // https://crbug.com/1148078
  if (_droppedScreenShotCopier)
    _droppedScreenShotCopier->CopyScreenShotsInDropData(dropData);

  _host->SetDropData(dropData);

  auto draggingInfo = DraggingInfo::New();
  [self populateDraggingInfo:draggingInfo.get() fromNSDraggingInfo:sender];
  uint32_t result = 0;
  _host->DraggingEntered(std::move(draggingInfo), &result);
  return result;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  if (!_host)
    return;
  _host->DraggingExited();
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  if (!_host)
    return NSDragOperationNone;
  auto draggingInfo = DraggingInfo::New();
  [self populateDraggingInfo:draggingInfo.get() fromNSDraggingInfo:sender];
  uint32_t result = 0;
  _host->DraggingUpdated(std::move(draggingInfo), &result);
  return result;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  if (!_host)
    return NO;
  auto draggingInfo = DraggingInfo::New();
  [self populateDraggingInfo:draggingInfo.get() fromNSDraggingInfo:sender];
  bool result = false;
  _host->PerformDragOperation(std::move(draggingInfo), &result);
  return result;
}

- (void)clearViewsHostableView {
  _viewsHostableView = nullptr;
}

- (void)setHost:(remote_cocoa::mojom::WebContentsNSViewHost*)host {
  if (!host)
    [_dragSource clearHostAndWebContentsView];
  _host = host;
}

- (void)viewDidBecomeFirstResponder:(NSNotification*)notification {
  if (!_host)
    return;

  NSView* view = [notification object];
  if (![[self subviews] containsObject:view])
    return;

  NSSelectionDirection ns_direction = static_cast<NSSelectionDirection>(
      [[notification userInfo][kSelectionDirection] unsignedIntegerValue]);

  SelectionDirection direction;
  switch (ns_direction) {
    case NSDirectSelection:
      direction = SelectionDirection::kDirect;
      break;
    case NSSelectingNext:
      direction = SelectionDirection::kForward;
      break;
    case NSSelectingPrevious:
      direction = SelectionDirection::kReverse;
      break;
    default:
      return;
  }
  _host->OnBecameFirstResponder(direction);
}

- (void)setWebContentsVisibility:(remote_cocoa::mojom::Visibility)visibility {
  if (_host && !content::GetContentClient()->browser()->IsShuttingDown())
    _host->OnWindowVisibilityChanged(visibility);
}

- (void)performDelayedSetWebContentsOccluded {
  _willSetWebContentsOccludedAfterDelay = NO;
  [self setWebContentsVisibility:remote_cocoa::mojom::Visibility::kOccluded];
}

- (void)cancelDelayedSetWebContentsOccluded {
  if (!_willSetWebContentsOccludedAfterDelay)
    return;

  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:@selector
                                     (performDelayedSetWebContentsOccluded)
                                       object:nil];
  _willSetWebContentsOccludedAfterDelay = NO;
}

- (BOOL)willSetWebContentsOccludedAfterDelayForTesting {
  return _willSetWebContentsOccludedAfterDelay;
}

- (void)updateWebContentsVisibility:
    (remote_cocoa::mojom::Visibility)visibility {
  using remote_cocoa::mojom::Visibility;

  DCHECK(base::FeatureList::IsEnabled(kMacWebContentsOcclusion));
  if (!_host)
    return;

  // When a web contents is marked something other than occluded, we want
  // to act on that right away. For the occluded state, the urgency is
  // lower and the cost of prematurely switching to the occluded state is
  // potentially significant. For example, if during browser startup our
  // window is initially obscured but will become main, our visibility
  // might be set to occluded and then quickly change to visible. Toggling
  // between these states would cause our web contents to throw away
  // resources it needs to display its content and then scramble to reacquire
  // those resources. There's no need to mark a web contents occluded right
  // away. Instead, we wait a bit and abort setting the web contents to
  // occluded if our window switches back to visible or hidden in the meantime.
  if (visibility != Visibility::kOccluded) {
    [self cancelDelayedSetWebContentsOccluded];
    _host->OnWindowVisibilityChanged(visibility);
    return;
  }

  if (_willSetWebContentsOccludedAfterDelay)
    return;

  // Coalesce one second's worth of occlusion updates.
  const NSTimeInterval kOcclusionUpdateDelayInSeconds = 1.0;
  [self performSelector:@selector(performDelayedSetWebContentsOccluded)
             withObject:nil
             afterDelay:kOcclusionUpdateDelayInSeconds];
  _willSetWebContentsOccludedAfterDelay = YES;
}

- (void)updateWebContentsVisibility {
  using remote_cocoa::mojom::Visibility;
  if (!_host)
    return;

  Visibility visibility = Visibility::kVisible;
  if ([self isHiddenOrHasHiddenAncestor] || ![self window])
    visibility = Visibility::kHidden;
  else if ([[self window] isOccluded])
    visibility = Visibility::kOccluded;

  [self updateWebContentsVisibility:visibility];
}

- (void)legacyUpdateWebContentsVisibility {
  using remote_cocoa::mojom::Visibility;
  DCHECK(!base::FeatureList::IsEnabled(kMacWebContentsOcclusion));
  if (!_host || _inFullScreenTransition)
    return;
  Visibility visibility = Visibility::kVisible;
  if ([self isHiddenOrHasHiddenAncestor] || ![self window])
    visibility = Visibility::kHidden;
  else if ([[self window] occlusionState] & NSWindowOcclusionStateVisible)
    visibility = Visibility::kVisible;
  else
    visibility = Visibility::kOccluded;
  _host->OnWindowVisibilityChanged(visibility);
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldBoundsSize {
  // Subviews do not participate in auto layout unless the the size this view
  // changes. This allows RenderWidgetHostViewMac::SetBounds(..) to select a
  // size of the subview that differs from its superview in preparation for an
  // upcoming WebContentsView resize.
  // See http://crbug.com/264207 and http://crbug.com/655112.
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];

  // Perform manual layout of subviews, e.g., when the window size changes.
  for (NSView* subview in [self subviews])
    [subview setFrame:[self bounds]];
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  NSWindow* oldWindow = [self window];

  if (base::FeatureList::IsEnabled(kMacWebContentsOcclusion)) {
    if (oldWindow) {
      [notificationCenter
          removeObserver:self
                    name:NSWindowDidChangeOcclusionStateNotification
                  object:oldWindow];
    }

    if (newWindow) {
      [notificationCenter
          addObserver:self
             selector:@selector(windowChangedOcclusionState:)
                 name:NSWindowDidChangeOcclusionStateNotification
               object:newWindow];
    }

    return;
  }

  _inFullScreenTransition = NO;
  if (oldWindow) {
    NSArray* notificationsToRemove = @[
      NSWindowDidChangeOcclusionStateNotification,
      NSWindowWillEnterFullScreenNotification,
      NSWindowDidEnterFullScreenNotification,
      NSWindowWillExitFullScreenNotification,
      NSWindowDidExitFullScreenNotification
    ];
    for (NSString* notificationName in notificationsToRemove) {
      [notificationCenter removeObserver:self
                                    name:notificationName
                                  object:oldWindow];
    }
  }
  if (newWindow) {
    [notificationCenter addObserver:self
                           selector:@selector(windowChangedOcclusionState:)
                               name:NSWindowDidChangeOcclusionStateNotification
                             object:newWindow];
    // The fullscreen transition causes spurious occlusion notifications.
    // See https://crbug.com/1081229
    [notificationCenter addObserver:self
                           selector:@selector(fullscreenTransitionStarted:)
                               name:NSWindowWillEnterFullScreenNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(fullscreenTransitionComplete:)
                               name:NSWindowDidEnterFullScreenNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(fullscreenTransitionStarted:)
                               name:NSWindowWillExitFullScreenNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(fullscreenTransitionComplete:)
                               name:NSWindowDidExitFullScreenNotification
                             object:newWindow];
  }
}

- (void)windowChangedOcclusionState:(NSNotification*)aNotification {
  if (!base::FeatureList::IsEnabled(kMacWebContentsOcclusion)) {
    [self legacyUpdateWebContentsVisibility];
    return;
  }

  // Only respond to occlusion notifications sent by the occlusion checker.
  NSDictionary* userInfo = [aNotification userInfo];
  NSString* occlusionCheckerKey = [WebContentsOcclusionCheckerMac className];
  if (userInfo[occlusionCheckerKey] != nil)
    [self updateWebContentsVisibility];
}

- (void)fullscreenTransitionStarted:(NSNotification*)notification {
  DCHECK(!base::FeatureList::IsEnabled(kMacWebContentsOcclusion));
  _inFullScreenTransition = YES;
}

- (void)fullscreenTransitionComplete:(NSNotification*)notification {
  DCHECK(!base::FeatureList::IsEnabled(kMacWebContentsOcclusion));
  _inFullScreenTransition = NO;
}

- (void)viewDidMoveToWindow {
  if (!base::FeatureList::IsEnabled(kMacWebContentsOcclusion)) {
    [self legacyUpdateWebContentsVisibility];
    return;
  }

  [self updateWebContentsVisibility];
}

- (void)viewDidHide {
  if (!base::FeatureList::IsEnabled(kMacWebContentsOcclusion)) {
    [self legacyUpdateWebContentsVisibility];
    return;
  }

  [self updateWebContentsVisibility];
}

- (void)viewDidUnhide {
  if (!base::FeatureList::IsEnabled(kMacWebContentsOcclusion)) {
    [self legacyUpdateWebContentsVisibility];
    return;
  }

  [self updateWebContentsVisibility];
}

// ViewsHostable protocol implementation.
- (ui::ViewsHostableView*)viewsHostableView {
  return _viewsHostableView;
}

@end

@implementation NSWindow (WebContentsViewCocoa)

// Collects the WebContentsViewCocoas contained in the view hierarchy
// rooted at `view` in the array `webContents`.
- (void)_addWebContentsViewCocoasFromView:(NSView*)view
                                  toArray:
                                      (NSMutableArray<WebContentsViewCocoa*>*)
                                          webContents
                           haltAfterFirst:(BOOL)haltAfterFirst {
  for (NSView* subview in [view subviews]) {
    if ([subview isKindOfClass:[WebContentsViewCocoa class]]) {
      [webContents addObject:(WebContentsViewCocoa*)subview];
      if (haltAfterFirst) {
        return;
      }
    } else {
      [self _addWebContentsViewCocoasFromView:subview
                                      toArray:webContents
                               haltAfterFirst:haltAfterFirst];
    }
  }
}

- (NSArray<WebContentsViewCocoa*>*)webContentsViewCocoa {
  NSMutableArray<WebContentsViewCocoa*>* webContents = [NSMutableArray array];

  [self _addWebContentsViewCocoasFromView:[self contentView]
                                  toArray:webContents
                           haltAfterFirst:NO];

  return webContents;
}

- (BOOL)containsWebContentsViewCocoa {
  NSMutableArray<WebContentsViewCocoa*>* webContents = [NSMutableArray array];

  [self _addWebContentsViewCocoasFromView:[self contentView]
                                  toArray:webContents
                           haltAfterFirst:YES];

  return webContents.count > 0;
}

@end
