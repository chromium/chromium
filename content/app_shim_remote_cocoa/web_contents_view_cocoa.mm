// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/web_contents_view_cocoa.h"

#include <AppKit/AppKit.h>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#import "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#import "content/app_shim_remote_cocoa/web_contents_occlusion_checker_mac.h"
#import "content/app_shim_remote_cocoa/web_drag_source_mac.h"
#import "content/browser/web_contents/web_contents_view_mac.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#include "content/common/features.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_util_mac.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/image/image.h"
#include "ui/resources/grit/ui_resources.h"

using content::DropData;
using features::kMacWebContentsOcclusion;
using remote_cocoa::mojom::DraggingInfo;
using remote_cocoa::mojom::SelectionDirection;

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
    if (!base::Contains(value, "/var")) {
      return false;
    }
    if (!base::Contains(value, "screencaptureui")) {
      return false;
    }
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
  // Instances of this class are owned by both `_host` and AppKit. The `_host`
  // must call `-setHost:nil` in its destructor.
  raw_ptr<remote_cocoa::mojom::WebContentsNSViewHost> _host;

  // The interface exported to views::Views that embed this as a sub-view.
  raw_ptr<ui::ViewsHostableView> _viewsHostableView;

  BOOL _mouseDownCanMoveWindow;

  // Utility to copy screenshots to a usable directory for PWAs. This utility
  // will maintain a temporary directory for such screenshot files until this
  // WebContents is destroyed.
  // https://crbug.com/1148078
  std::unique_ptr<remote_cocoa::DroppedScreenShotCopierMac>
      _droppedScreenShotCopier;

  // Drag variables.
  WebDragSource* __strong _dragSource;
  NSDragOperation _dragOperation;

  gfx::Rect _windowControlsOverlayRect;

  // TODO(crbug.com/40593221): Remove this when kMacWebContentsOcclusion
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
  self = [super initWithFrame:NSZeroRect tracking:YES];
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

  NSPoint screenPoint = [self.window convertPointToScreen:windowPoint];
  NSRect screenFrame = self.window.screen.frame;
  info->location_in_screen =
      gfx::PointF(screenPoint.x, screenFrame.size.height - screenPoint.y);

  NSPasteboard* pboard = [nsInfo draggingPasteboard];
  NSArray<URLAndTitle*>* urls_and_titles =
      ui::clipboard_util::URLsAndTitlesFromPasteboard(pboard,
                                                      /*include_files=*/true);

  if (urls_and_titles.count) {
    info->url = GURL(base::SysNSStringToUTF8(urls_and_titles.firstObject.URL));
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
    ui::kUTTypeChromiumInitiatedDrag, ui::kUTTypeChromiumDataTransferCustomData,
    ui::kUTTypeWebKitWebURLsWithTitles
  ]];
}

- (void)mouseEvent:(NSEvent*)theEvent {
  if (!_host)
    return;
  _host->OnMouseEvent(ui::EventFromNative(base::apple::OwnedNSEvent(theEvent)));
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
                 sourceOrigin:(const url::Origin&)sourceOrigin
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset
                 isPrivileged:(BOOL)isPrivileged {
  if (!_host)
    return;

  NSPoint mouseLocation = [self.window mouseLocationOutsideOfEventStream];
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
                                          location:mouseLocation
                                     modifierFlags:0
                                         timestamp:NSApp.currentEvent.timestamp
                                      windowNumber:self.window.windowNumber
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  _dragSource = [[WebDragSource alloc] initWithHost:_host
                                           dropData:dropData
                                       sourceOrigin:sourceOrigin
                                       isPrivileged:isPrivileged];
  NSDraggingItem* draggingItem =
      [[NSDraggingItem alloc] initWithPasteboardWriter:_dragSource];

  if (!image) {
    image = content::GetContentClient()
                ->GetNativeImageNamed(IDR_DEFAULT_FAVICON)
                .ToNSImage();
  }

  // The frame given to -[NSDraggingItem setDraggingFrame:contents:] will be
  // interpreted as being in the coordinate system of this view, so convert it
  // from the coordinate system of the window.
  mouseLocation = [self convertPoint:mouseLocation fromView:nil];
  NSRect imageRect = NSMakeRect(mouseLocation.x, mouseLocation.y,
                                image.size.width, image.size.height);
  imageRect.origin.x -= offset.x;
  // Deal with Cocoa's flipped coordinate system.
  imageRect.origin.y -= image.size.height - offset.y;
  [draggingItem setDraggingFrame:imageRect contents:image];

  _dragOperation = operationMask;

  // Run the drag operation.
  [self beginDraggingSessionWithItems:@[ draggingItem ]
                                event:dragEvent
                               source:self];
}

// NSDraggingSource methods

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return _dragOperation;
}

// Called when a drag initiated in our view ends.
- (void)draggingSession:(NSDraggingSession*)session
           endedAtPoint:(NSPoint)screenPoint
              operation:(NSDragOperation)operation {
  if (!_host) {
    return;
  }

  NSPoint localPoint = NSZeroPoint;
  if (self.window) {
    NSPoint basePoint = [self.window convertPointFromScreen:screenPoint];
    localPoint = [self convertPoint:basePoint fromView:nil];
  }

  // Flip the two points as per Cocoa's coordinate system.
  NSRect viewFrame = self.frame;
  NSRect screenFrame = self.window.screen.frame;
  _host->EndDrag(
      operation,
      gfx::PointF(localPoint.x, viewFrame.size.height - localPoint.y),
      gfx::PointF(screenPoint.x, screenFrame.size.height - screenPoint.y));

  // The drag is complete. Disconnect the drag source.
  [_dragSource webContentsIsGone];
  _dragSource = nil;
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  if (!_host)
    return NSDragOperationNone;

  // Fill out a DropData from pasteboard.
  DropData dropData =
      content::PopulateDropDataFromPasteboard(sender.draggingPasteboard);

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
  if (!host) {
    [_dragSource webContentsIsGone];
  }
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
  if (_host && !(content::GetContentClient()->browser() &&
                 content::GetContentClient()->browser()->IsShuttingDown())) {
    _host->OnWindowVisibilityChanged(visibility);
  }
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

- (void)updateWindowControlsOverlay:(const gfx::Rect&)boundingRect {
  _windowControlsOverlayRect = boundingRect;
}

- (NSView*)hitTest:(NSPoint)point {
  if (!_windowControlsOverlayRect.IsEmpty()) {
    // _windowControlsOverlayRect represents the area at the top of the web
    // contents that is available for the web. As such, if the y coordinate
    // falls within this rect, but the x coordinate doesn't we want to route
    // events to the BridgedContentView (our superview) instead.
    gfx::Point p = gfx::Point(point);
    p.set_y(NSHeight(self.bounds) - p.y());
    if (p.y() >= _windowControlsOverlayRect.y() &&
        p.y() < _windowControlsOverlayRect.bottom() &&
        (p.x() < _windowControlsOverlayRect.x() ||
         p.x() >= _windowControlsOverlayRect.right())) {
      return self.superview;
    }
  }
  return [super hitTest:point];
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
