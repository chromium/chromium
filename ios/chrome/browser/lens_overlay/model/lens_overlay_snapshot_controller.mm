// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"

#import <map>

#import "base/task/bind_post_task.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/model/snapshot_cover_view_controller.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// The number of rows the infill mechanism should take into account when
// computing the average color.
CGFloat const kNumberOfRowsForInfill = 5;

// Number of bytes per pixel.
size_t const kBytesPerPixel = 4;

}  // namespace

#pragma mark - Image Processing Helpers

// A color is considered dominant if it represents more than 50% of the total
// pixels.
//
// This method returns `nil` if no such color exists.
UIColor* DominantColor(UIImage* image) {
  CGImageRef imageRef = [image CGImage];

  size_t width = CGImageGetWidth(imageRef);
  size_t height = CGImageGetHeight(imageRef);
  size_t numberOfPixels = width * height;
  CGFloat dominantColorThreshold = numberOfPixels / 2;

  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  uint8_t* rawData = new uint8_t[numberOfPixels * kBytesPerPixel]();

  NSUInteger bytesPerRow = kBytesPerPixel * width;
  CGContextRef context =
      CGBitmapContextCreate(rawData, width, height, 8, bytesPerRow, colorSpace,
                            (uint32_t)kCGImageAlphaPremultipliedLast |
                                (uint32_t)kCGImageByteOrder32Big);
  CGColorSpaceRelease(colorSpace);
  CGContextDrawImage(context, CGRectMake(0, 0, width, height), imageRef);
  CGContextRelease(context);

  std::map<uint32_t, uint32_t> colorsMapping;

  size_t columnIndex = 0;
  size_t rowIndex = 0;
  for (size_t n = 0; n < numberOfPixels; ++n) {
    size_t index = (bytesPerRow * rowIndex) + columnIndex * kBytesPerPixel;

    uint32_t red = rawData[index];
    uint32_t green = rawData[index + 1];
    uint32_t blue = rawData[index + 2];
    uint32_t alpha = rawData[index + 3];
    uint32_t color = (red << 24) | (green << 16) | (blue << 8) | alpha;

    uint32_t colorCount = colorsMapping[color] + 1;
    if (colorCount >= dominantColorThreshold) {
      delete[] rawData;
      return [[UIColor alloc] initWithRed:CGFloat(red) / 256
                                    green:CGFloat(green) / 256
                                     blue:CGFloat(blue) / 256
                                    alpha:CGFloat(alpha) / 256];
    }

    colorsMapping[color] = colorCount;

    rowIndex++;
    if (rowIndex == height) {
      rowIndex = 0;
      ++columnIndex;
    }
  }

  delete[] rawData;
  return nil;
}

// Reduces the image to the average color, by factoring in all 3 color channels.
//
// This method can return `nil`.
UIColor* AverageColor(UIImage* image) {
  CIImage* inputImage = [[CIImage alloc] initWithImage:image];

  if (!inputImage) {
    return nil;
  }

  CIVector* extentVector =
      [[CIVector alloc] initWithX:inputImage.extent.origin.x
                                Y:inputImage.extent.origin.y
                                Z:inputImage.extent.size.width
                                W:inputImage.extent.size.height];

  CIFilter* filter = [CIFilter filterWithName:@"CIAreaAverage"
                          withInputParameters:@{
                            kCIInputImageKey : inputImage,
                            kCIInputExtentKey : extentVector
                          }];
  CIImage* outputImage = filter.outputImage;
  CIContext* context = [CIContext
      contextWithOptions:@{kCIContextWorkingColorSpace : (id)kCFNull}];

  uint8_t bitmap[4];
  [context render:outputImage
         toBitmap:&bitmap
         rowBytes:4
           bounds:CGRectMake(0, 0, 1, 1)
           format:kCIFormatRGBA8
       colorSpace:nil];

  return [UIColor colorWithRed:CGFloat(bitmap[0]) / 255
                         green:CGFloat(bitmap[1]) / 255
                          blue:CGFloat(bitmap[2]) / 255
                         alpha:CGFloat(bitmap[3]) / 255];
}

// Crop the image on top for the first `numberOfRows` rows (measured in points).
UIImage* ImageCroppedToFirstRows(UIImage* image, int numberOfRows) {
  CGRect croppingRect = CGRectMake(0, 0, image.size.width * image.scale,
                                   numberOfRows * image.scale);
  return [[UIImage alloc] initWithCGImage:CGImageCreateWithImageInRect(
                                              image.CGImage, croppingRect)];
}

// Captures a snapshot of the given `UIWindow`.
UIImage* CaptureSnapshotOfWindow(UIWindow* window) {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:window.bounds.size];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [window drawViewHierarchyInRect:window.bounds afterScreenUpdates:NO];
  }];
}

// Creates a new window that displays a static image of the original window.
UIWindow* CreateMirrorWindowFromBaseWindow(
    UIWindow* window,
    base::OnceClosure windowShownCallback) {
  UIWindow* mirrorWindow =
      [[UIWindow alloc] initWithWindowScene:window.windowScene];
  UIImage* windowSnapshot = CaptureSnapshotOfWindow(window);
  mirrorWindow.rootViewController = [[SnapshotCoverViewController alloc]
      initWithImage:windowSnapshot
      onFirstAppear:base::CallbackToBlock(std::move(windowShownCallback))];

  return mirrorWindow;
}

// Extends the top and bottom edges of the raw snapshot to match the window
// dimensions.
//
// The top edge is filled with the most prominent color found at the top of the
// original snapshot. The bottom edge is extended using the background color of
// the UI elements.
void PreprocessSnapshot(UIImage* snapshot,
                        CGSize expected_snapshot_size,
                        UIEdgeInsets viewport_insets,
                        base::OnceCallback<void(UIImage*)> callback) {
  // The color used by the omnibox and the bottom toolbar.
  UIColor* elementsBackgroundColor = [UIColor colorNamed:kBackgroundColor];

  // We take a portion of the top header of the snapshot as our reference.
  // It is important to take more than one row into account when computing the
  // infill colors to reduce noice (e.g. some webistes add a 1px horizontal rule
  // at the very top of the viewport).
  UIImage* croppedImageHeader =
      ImageCroppedToFirstRows(snapshot, kNumberOfRowsForInfill);
  // Use the dominant color if one exists. A dominant color is defined as a
  // color with a pixel count exceeding 50% of the total pixel count, ensuring
  // its majority status.
  UIColor* topInfillColor = DominantColor(croppedImageHeader);
  // If no such dominant color exists, fallback to the average color, which aims
  // to minimise the contrast between the top infill and the upper part of the
  // snapshot. This is particularry useful when snapshoting images.
  if (!topInfillColor) {
    topInfillColor = AverageColor(croppedImageHeader);
  }
  // If the average failed, infill with the background color as the last resort.
  if (!topInfillColor) {
    topInfillColor = elementsBackgroundColor;
  }

  // The snapshot taken was only of the visible content on the screen. To make
  // it appear fullscreen, add a solid color fill at the top and bottom of the
  // image corresponding to the initial insets.
  UIColor* bottomInfillColor = elementsBackgroundColor;
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:expected_snapshot_size];
  UIImage* snapshotWithInfill =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        [topInfillColor setFill];
        UIRectFill(context.format.bounds);
        CGSize size = context.format.bounds.size;
        CGPoint origin = context.format.bounds.origin;
        CGRect rect = CGRectMake(origin.x, origin.y + size.height / 2,
                                 size.width, size.height / 2);
        [bottomInfillColor setFill];
        UIRectFill(rect);
        [snapshot drawAtPoint:CGPointMake(0, viewport_insets.top)];
      }];

  // Lens requires the image to be 1.0 scale.
  UIImage* rescaledSnapshot =
      [[UIImage alloc] initWithCGImage:snapshotWithInfill.CGImage
                                 scale:1
                           orientation:UIImageOrientationUp];

  std::move(callback).Run(rescaledSnapshot);
}

#pragma mark - LensOverlaySnapshotController

LensOverlaySnapshotController::LensOverlaySnapshotController(
    SnapshotTabHelper* snapshot_tab_helper,
    FullscreenController* fullscreen_controller,
    UIWindow* window,
    bool is_bottom_omnibox)
    : snapshot_tab_helper_(snapshot_tab_helper),
      fullscreen_controller_(fullscreen_controller),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      base_window_(window),
      is_bottom_omnibox_(is_bottom_omnibox) {}

LensOverlaySnapshotController::~LensOverlaySnapshotController() {
  FinalizeCapturing();
  pending_snapshot_callbacks_.clear();
}

UIImage* LensOverlaySnapshotController::CropSnapshotToWindowSafeArea(
    UIImage* snapshot) {
  if (!base_window_) {
    return snapshot;
  }

  UIEdgeInsets viewportInsets = base_window_.safeAreaInsets;
  CGRect croppingRect = CGRectMake(
      viewportInsets.left * snapshot.scale, viewportInsets.top * snapshot.scale,
      (base_window_.bounds.size.width - viewportInsets.right) * snapshot.scale,
      (base_window_.bounds.size.height - viewportInsets.bottom) *
          snapshot.scale);

  return [[UIImage alloc] initWithCGImage:CGImageCreateWithImageInRect(
                                              snapshot.CGImage, croppingRect)];
}

UIImage* LensOverlaySnapshotController::CaptureSnapshotOfBaseWindow() {
  if (!base_window_) {
    return nil;
  }

  return CaptureSnapshotOfWindow(base_window_);
}

void LensOverlaySnapshotController::CaptureFullscreenSnapshot(
    SnapshotCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_snapshot_callbacks_.push_back(std::move(callback));

  // If there is a capture in progress wait for it to complete.
  if (is_capturing_) {
    return;
  }

  // All the steps should be synchronized on the same sequence as the one that
  // initiated the capture.
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  BeginCapturing();
  expected_window_size_ = base_window_.bounds.size;

  // When the omnibox is positioned at the top, the infill is evenly distributed
  // on top and bottom, removing the need to enter fullscreen mode."
  if (!is_bottom_omnibox_) {
    ShowStaticSnapshotOfBaseWindowIfNeeded();
    return;
  }

  // If fullscreen is already enabled directly take the screenshot.
  bool is_already_fullscreen = fullscreen_controller_->GetProgress() == 0.0;
  if (is_already_fullscreen) {
    ShowStaticSnapshotOfBaseWindowIfNeeded();
    return;
  }

  // Register as observer and request fullscreen.
  fullscreen_controller_->AddObserver(this);
  if (fullscreen_controller_->IsEnabled()) {
    // Enter fullscreen and rely on the update from the fullscreen controller.
    fullscreen_controller_->EnterFullscreen();
  } else {
    // Fullscreen could not be requested, likely because the content is too
    // small to enlarge the view. Go straight to fetching a screenshot.
    ShowStaticSnapshotOfBaseWindowIfNeeded();
  }
}

void LensOverlaySnapshotController::FullscreenDidAnimate(
    FullscreenController* controller,
    FullscreenAnimatorStyle style) {
  DCHECK(controller == this->fullscreen_controller_);

  // Progress of 0.0 means that the toolbar is completely hidden.
  bool is_fullscreen = fullscreen_controller_->GetProgress() == 0.0;
  if (!is_fullscreen) {
    return;
  }

  task_tracker_.PostTask(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&LensOverlaySnapshotController::
                         ShowStaticSnapshotOfBaseWindowIfNeeded,
                     weak_ptr_factory_.GetWeakPtr()));
}

// The inset amount of the content relative to the device screen when the
// snapshot is taken.
UIEdgeInsets
LensOverlaySnapshotController::GetContentInsetsOnSnapshotCapture() {
  if (!is_bottom_omnibox_ || is_NTP_) {
    return fullscreen_controller_->GetMaxViewportInsets();
  }

  return fullscreen_controller_->IsEnabled()
             ? fullscreen_controller_->GetMinViewportInsets()
             : fullscreen_controller_->GetMaxViewportInsets();
}

UIEdgeInsets LensOverlaySnapshotController::GetSnapshotInsets() {
  // The NTP does not require snapshot insetting.
  if (is_NTP_) {
    return UIEdgeInsetsZero;
  }

  // If the fullscreen mode is achieved by adjusting the size of the scroll
  // view, the WebState view is already positioned correctly within the viewable
  // area and doesn't require any further adjustments.
  //
  // Note: In practice, this condition is true only when fullscreen smooth
  // scrolling of the default view port is disabled.
  if (fullscreen_controller_->ResizesScrollView()) {
    return UIEdgeInsetsZero;
  }

  // If the fullscreen mode is implemented using content insets, the WebState
  // view needs to be adjusted inwards by the viewport insets.
  return GetContentInsetsOnSnapshotCapture();
}

bool LensOverlaySnapshotController::ShouldShowStaticSnapshot() {
  if (is_NTP_ || is_pdf_document_) {
    return false;
  }
  return true;
}

// Fullscreen has got to a steady state, either by already being in a fullscreen
// state, completing an animation or being unable to change state.
// Regardless, a screenshot can be taken when such state is reached. In
// preparation for the snapshotting flow, show a static image in a separate
// window to avoid the visual flickering caused by hdiing the original window.
void LensOverlaySnapshotController::ShowStaticSnapshotOfBaseWindowIfNeeded() {
  if (!is_capturing_) {
    return;
  }

  if (!ShouldShowStaticSnapshot()) {
    StartSnapshotFlow();
    return;
  }

  base::OnceClosure snapshotCapturedCallback =
      base::BindOnce(&LensOverlaySnapshotController::StartSnapshotFlow,
                     weak_ptr_factory_.GetWeakPtr());

  auto callbackOnWorkingSequence = base::BindPostTask(
      task_runner_.get(), std::move(snapshotCapturedCallback));

  // To have videos appear in snapshot, the window which contains the content
  // has to be momentarily hidden during the capturing process.
  //
  // To prevent the screen from briefly going black when the window is hidden
  // during capture, a temporary 'mirror' window is displayed. This mirror
  // window shows a still image of the original window, preventing any
  // visual flickering.
  //
  // This workaround is based on WebKit's internal snapshotting mechanism.
  mirror_window_ = CreateMirrorWindowFromBaseWindow(
      base_window_, std::move(callbackOnWorkingSequence));
  mirror_window_.windowLevel = base_window_.windowLevel + 1;
  mirror_window_.hidden = NO;
}

void LensOverlaySnapshotController::StartSnapshotFlow() {
  if (!is_capturing_) {
    return;
  }

  base::OnceCallback<void(UIImage*)> snapshotCapturedCallback =
      base::BindOnce(&LensOverlaySnapshotController::ProcessRawSnapshot,
                     weak_ptr_factory_.GetWeakPtr());

  auto callbackOnWorkingSequence = base::BindPostTask(
      task_runner_.get(), std::move(snapshotCapturedCallback));

  if (ShouldShowStaticSnapshot()) {
    base_window_.hidden = YES;
  }

  // TODO(crbug.com/365732763): Replace call to `UpdateSnapshotWithCallback`
  // once the new API method is exposed.
  snapshot_tab_helper_->UpdateSnapshotWithCallback(
      base::CallbackToBlock(std::move(callbackOnWorkingSequence)));
}

void LensOverlaySnapshotController::ProcessRawSnapshot(UIImage* snapshot) {
  if (!is_capturing_) {
    return;
  }

  if (ShouldShowStaticSnapshot()) {
    base_window_.hidden = NO;
    mirror_window_.windowLevel = base_window_.windowLevel - 1;
  }

  base::OnceCallback<void(UIImage*)> snapshotCapturedCallback =
      base::BindOnce(&LensOverlaySnapshotController::NotifySnapshotComplete,
                     weak_ptr_factory_.GetWeakPtr());
  auto callbackOnInitialSequence = base::BindPostTask(
      task_runner_.get(), std::move(snapshotCapturedCallback));

  scoped_refptr<base::SequencedTaskRunner> backgroundRunner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE});

  UIEdgeInsets viewportInsets = GetContentInsetsOnSnapshotCapture();
  // On NTP we cannot rely on the top component of the content insets.
  if (is_NTP_) {
    // However, as the snapshot is fullscreen, the top inset is computed as the
    // remaining space after accounting for the bottom inset and the raw
    // snapshot height.
    CGFloat topInsetForNTP = expected_window_size_.height -
                             viewportInsets.bottom - snapshot.size.height;
    viewportInsets.top = topInsetForNTP;
  }

  auto preprocessCallback =
      base::BindOnce(&PreprocessSnapshot, snapshot, expected_window_size_,
                     viewportInsets, std::move(callbackOnInitialSequence));
  backgroundRunner->PostTask(FROM_HERE, std::move(preprocessCallback));
}

void LensOverlaySnapshotController::NotifySnapshotComplete(UIImage* snapshot) {
  FinalizeCapturing();

  // Consume and clear the pending callbacks storage.
  std::vector<SnapshotCallback> pending_snapshot_callbacks_copy;
  for (auto& callback : pending_snapshot_callbacks_) {
    pending_snapshot_callbacks_copy.push_back(std::move(callback));
  }
  pending_snapshot_callbacks_.clear();

  for (auto& callback : pending_snapshot_callbacks_copy) {
    std::move(callback).Run(snapshot);
  }
}

void LensOverlaySnapshotController::ReleaseAuxiliaryWindows() {
  mirror_window_ = nil;
}

void LensOverlaySnapshotController::CancelOngoingCaptures() {
  task_tracker_.TryCancelAll();
  NotifySnapshotComplete(nil);
}

void LensOverlaySnapshotController::BeginCapturing() {
  is_capturing_ = true;
  if (delegate_) {
    delegate_->OnSnapshotCaptureBegin();
  }
}

void LensOverlaySnapshotController::FinalizeCapturing() {
  fullscreen_controller_->RemoveObserver(this);
  is_capturing_ = false;
  if (delegate_) {
    delegate_->OnSnapshotCaptureEnd();
  }
}
