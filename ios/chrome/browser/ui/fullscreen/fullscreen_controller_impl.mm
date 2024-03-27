// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_impl.h"

#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/download/model/mime_type_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_system_notification_observer.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"

// static
FullscreenController* FullscreenController::FromBrowser(Browser* browser) {
  // TODO(crbug.com/1469841): Do not create FullscreenController and
  // FullscreenWebStateListObserver for an inactive browser.
  FullscreenController* fullscreen_controller =
      static_cast<FullscreenController*>(
          browser->GetUserData(FullscreenController::UserDataKey()));
  if (!fullscreen_controller) {
    fullscreen_controller = new FullscreenControllerImpl(browser);
    browser->SetUserData(FullscreenController::UserDataKey(),
                         base::WrapUnique(fullscreen_controller));
  }
  return fullscreen_controller;
}

FullscreenControllerImpl::FullscreenControllerImpl(Browser* browser)
    : broadcaster_([[ChromeBroadcaster alloc] init]),
      mediator_(this, &model_),
      web_state_list_observer_(this, &model_, &mediator_),
      fullscreen_browser_observer_(&web_state_list_observer_, browser),
      bridge_([[ChromeBroadcastOberverBridge alloc] initWithObserver:&model_]),
      notification_observer_([[FullscreenSystemNotificationObserver alloc]
          initWithController:this
                    mediator:&mediator_]) {
  DCHECK(broadcaster_);
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewContentSize:)];
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewSize:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewIsScrolling:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewIsDragging:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewIsZooming:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastScrollViewContentInset:)];
    [broadcaster_ addObserver:bridge_
                  forSelector:@selector(broadcastContentScrollOffset:)];
  }
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastCollapsedTopToolbarHeight:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastExpandedTopToolbarHeight:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastCollapsedBottomToolbarHeight:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
}

FullscreenControllerImpl::~FullscreenControllerImpl() {
  mediator_.Disconnect();
  web_state_list_observer_.Disconnect();
  [notification_observer_ disconnect];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewContentSize:)];
  if (base::FeatureList::IsEnabled(web::features::kSmoothScrollingDefault)) {
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewSize:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewIsScrolling:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewIsDragging:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewIsZooming:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastScrollViewContentInset:)];
    [broadcaster_ removeObserver:bridge_
                     forSelector:@selector(broadcastContentScrollOffset:)];
  }
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastCollapsedTopToolbarHeight:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastExpandedTopToolbarHeight:)];
  [broadcaster_
      removeObserver:bridge_
         forSelector:@selector(broadcastExpandedBottomToolbarHeight:)];
  [broadcaster_
      removeObserver:bridge_
         forSelector:@selector(broadcastCollapsedBottomToolbarHeight:)];
}

ChromeBroadcaster* FullscreenControllerImpl::broadcaster() {
  return broadcaster_;
}

void FullscreenControllerImpl::AddObserver(
    FullscreenControllerObserver* observer) {
  mediator_.AddObserver(observer);
}

void FullscreenControllerImpl::RemoveObserver(
    FullscreenControllerObserver* observer) {
  mediator_.RemoveObserver(observer);
}

bool FullscreenControllerImpl::IsEnabled() const {
  return model_.enabled();
}

void FullscreenControllerImpl::IncrementDisabledCounter() {
  model_.IncrementDisabledCounter();
}

void FullscreenControllerImpl::DecrementDisabledCounter() {
  model_.DecrementDisabledCounter();
}

bool FullscreenControllerImpl::ResizesScrollView() const {
  return model_.ResizesScrollView();
}

void FullscreenControllerImpl::BrowserTraitCollectionChangedBegin() {
  mediator_.SetIsBrowserTraitCollectionUpdating(true);
}

void FullscreenControllerImpl::BrowserTraitCollectionChangedEnd() {
  mediator_.SetIsBrowserTraitCollectionUpdating(false);
}

CGFloat FullscreenControllerImpl::GetProgress() const {
  return model_.progress();
}

UIEdgeInsets FullscreenControllerImpl::GetMinViewportInsets() const {
  return model_.min_toolbar_insets();
}

UIEdgeInsets FullscreenControllerImpl::GetMaxViewportInsets() const {
  return model_.max_toolbar_insets();
}

UIEdgeInsets FullscreenControllerImpl::GetCurrentViewportInsets() const {
  return model_.current_toolbar_insets();
}

void FullscreenControllerImpl::EnterFullscreen() {
  base::RecordAction(base::UserMetricsAction("MobileFullscreenEntered"));
  mediator_.EnterFullscreen();
}

void FullscreenControllerImpl::ExitFullscreen() {
  base::RecordAction(base::UserMetricsAction("MobileFullscreenExited"));
  mediator_.ExitFullscreen();
}

void FullscreenControllerImpl::ExitFullscreenWithoutAnimation() {
  base::RecordAction(base::UserMetricsAction("MobileFullscreenExited"));
  mediator_.ExitFullscreenWithoutAnimation();
}

bool FullscreenControllerImpl::IsForceFullscreenMode() const {
  return model_.IsForceFullscreenMode();
}

void FullscreenControllerImpl::EnterForceFullscreenMode() {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  if (IsForceFullscreenMode()) {
    return;
  }
  model_.SetForceFullscreenMode(true);
  // Disable fullscreen because:
  // - It interfers with the animation when moving the secondary toolbar above
  // the keyboard.
  // - Fullscreen should not resize the toolbar it's above the keyboard.
  IncrementDisabledCounter();
  mediator_.ForceEnterFullscreen();
}

void FullscreenControllerImpl::ExitForceFullscreenMode() {
  CHECK(IsBottomOmniboxSteadyStateEnabled());
  if (!IsForceFullscreenMode()) {
    return;
  }
  DecrementDisabledCounter();
  model_.SetForceFullscreenMode(false);
  mediator_.ExitFullscreenWithoutAnimation();
}

void FullscreenControllerImpl::ResizeHorizontalViewport() {
  // TODO(crbug.com/1114054) this hack temporarily force change webview's
  // width insets to trigger a width recomputation of its content. It will cause
  // two relayouts.
  mediator_.ResizeHorizontalInsets();
}

void FullscreenControllerImpl::LogMimeTypeWhenExitFullscreen(
    web::WebState* webState) {
  const std::string& mimeType = webState->GetContentsMimeType();
  FullscreenMimeType type = FullscreenMimeType::kOther;

  if (mimeType == kAnimatedPortableNetworkGraphicsImageMimeType) {
    type = FullscreenMimeType::kAnimatedPortableNetworkGraphics;
  } else if (mimeType == kAVIFImageMimeType) {
    type = FullscreenMimeType::kAVIFImage;
  } else if (mimeType == kGenericBitmapMimeType) {
    type = FullscreenMimeType::kBitmap;
  } else if (mimeType == kCascadingStyleSheetMimeType) {
    type = FullscreenMimeType::kCSS;
  } else if (mimeType == kCommaSeparatedValuesMimeType) {
    type = FullscreenMimeType::kCSV;
  } else if (mimeType == kMicrosoftWordMimeType) {
    type = FullscreenMimeType::kMicrosoftWord;
  } else if (mimeType == kMicrosoftWordXMLMimeType) {
    type = FullscreenMimeType::kMicrosoftWordXML;
  } else if (mimeType == kGraphicsInterchangeFormatMimeType) {
    type = FullscreenMimeType::kGIF;
  } else if (mimeType == kHyperTextMarkupLanguageMimeType) {
    type = FullscreenMimeType::kHTML;
  } else if (mimeType == kIconFormatMimeType) {
    type = FullscreenMimeType::kIcon;
  } else if (mimeType == kJPEGImageMimeType) {
    type = FullscreenMimeType::kJPEG;
  } else if (mimeType == kJavaScriptMimeType) {
    type = FullscreenMimeType::kJS;
  } else if (mimeType == kJSONFormatMimeType) {
    type = FullscreenMimeType::kJSON;
  } else if (mimeType == kJSONLDFormatMimeType) {
    type = FullscreenMimeType::kJSONLD;
  } else if (mimeType == kPortableNetworkGraphicMimeType) {
    type = FullscreenMimeType::kPNG;
  } else if (mimeType == kAdobePortableDocumentFormatMimeType) {
    type = FullscreenMimeType::kPDF;
  } else if (mimeType == kHypertextPreprocessorMimeType) {
    type = FullscreenMimeType::kPHP;
  } else if (mimeType == kMicrosoftPowerPointMimeType) {
    type = FullscreenMimeType::kPowerPoint;
  } else if (mimeType == kMicrosoftPowerPointOpenXMLMimeType) {
    type = FullscreenMimeType::kPowerPointXML;
  } else if (mimeType == kRichTextFormatMimeType) {
    type = FullscreenMimeType::kRichTextFormat;
  } else if (mimeType == kScalableVectorGraphicMimeType) {
    type = FullscreenMimeType::kSVG;
  } else if (mimeType == kTaggedImageFileFormatMimeType) {
    type = FullscreenMimeType::kTIFF;
  } else if (mimeType == kTextMimeType) {
    type = FullscreenMimeType::kPlainText;
  } else if (mimeType == kWEBPImageMimeType) {
    type = FullscreenMimeType::kWebp;
  } else if (mimeType == kXHTMLMimeType) {
    type = FullscreenMimeType::kXHTML;
  } else if (mimeType == kMicrosoftExcelMimeType) {
    type = FullscreenMimeType::kMicrosoftExcel;
  } else if (mimeType == kMicrosoftExcelOpenXMLMimeType) {
    type = FullscreenMimeType::kMicrosoftExcelXML;
  } else if (mimeType == kXMLMimeType) {
    type = FullscreenMimeType::kXML;
  }

  base::UmaHistogramEnumeration("IOS.Fullscreen.ExitedMimeType", type);
}
