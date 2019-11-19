// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/validation_message_overlay_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

// ChromeClient for an internal page of ValidationMessageOverlayDelegate.
class ValidationMessageChromeClient : public EmptyChromeClient {
 public:
  explicit ValidationMessageChromeClient(ChromeClient& main_chrome_client,
                                         LocalFrameView* anchor_view)
      : main_chrome_client_(main_chrome_client), anchor_view_(anchor_view) {}

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(main_chrome_client_);
    visitor->Trace(anchor_view_);
    EmptyChromeClient::Trace(visitor);
  }

  void ScheduleAnimation(const LocalFrameView*,
                         base::TimeDelta delay = base::TimeDelta()) override {
    // Need to pass LocalFrameView for the anchor element because the Frame for
    // this overlay doesn't have an associated WebFrameWidget, which schedules
    // animation.
    main_chrome_client_->ScheduleAnimation(anchor_view_, delay);
  }

  float WindowToViewportScalar(LocalFrame* local_frame,
                               const float scalar_value) const override {
    return main_chrome_client_->WindowToViewportScalar(local_frame,
                                                       scalar_value);
  }

 private:
  Member<ChromeClient> main_chrome_client_;
  Member<LocalFrameView> anchor_view_;
};

ValidationMessageOverlayDelegate::ValidationMessageOverlayDelegate(
    Page& main_page,
    const Element& anchor,
    const String& message,
    TextDirection message_dir,
    const String& sub_message,
    TextDirection sub_message_dir)
    : main_page_(main_page),
      anchor_(anchor),
      message_(message),
      sub_message_(sub_message),
      message_dir_(message_dir),
      sub_message_dir_(sub_message_dir) {}

ValidationMessageOverlayDelegate::~ValidationMessageOverlayDelegate() {
  if (page_) {
    // This function can be called in EventDispatchForbiddenScope for the main
    // document, and the following operations dispatch some events. It's safe
    // because the page can't listen the events.
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
    page_->WillBeDestroyed();
  }
}

LocalFrameView& ValidationMessageOverlayDelegate::FrameView() const {
  DCHECK(page_)
      << "Do not call FrameView() before the first call of CreatePage()";
  return *To<LocalFrame>(page_->MainFrame())->View();
}

void ValidationMessageOverlayDelegate::PaintFrameOverlay(
    const FrameOverlay& overlay,
    GraphicsContext& context,
    const IntSize& view_size) const {
  if (IsHiding() && !page_)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, overlay,
                                                  DisplayItem::kFrameOverlay))
    return;
  DrawingRecorder recorder(context, overlay, DisplayItem::kFrameOverlay);

  const_cast<ValidationMessageOverlayDelegate*>(this)->UpdateFrameViewState(
      overlay, view_size);

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    context.DrawRecord(FrameView().GetPaintRecord());
  } else {
    // The overlay frame is has a standalone paint property tree. Paint it in
    // its root space into a paint record, then draw the record into the proper
    // target space in the overlaid frame.
    PaintRecordBuilder paint_record_builder(nullptr, &context);
    FrameView().PaintOutsideOfLifecycle(paint_record_builder.Context(),
                                        kGlobalPaintNormalPhase);
    context.DrawRecord(paint_record_builder.EndRecording());
  }
}

void ValidationMessageOverlayDelegate::ServiceScriptedAnimations(
    base::TimeTicks monotonic_frame_begin_time) {
  page_->Animator().ServiceScriptedAnimations(monotonic_frame_begin_time);
}

void ValidationMessageOverlayDelegate::UpdateFrameViewState(
    const FrameOverlay& overlay,
    const IntSize& view_size) {
  if (FrameView().Size() != view_size) {
    FrameView().Resize(view_size);
    page_->GetVisualViewport().SetSize(view_size);
  }
  IntRect intersection = overlay.Frame().RemoteViewportIntersection();
  AdjustBubblePosition(intersection.IsEmpty() ? IntRect(IntPoint(), view_size)
                                              : intersection);

  // This manual invalidation is necessary to avoid a DCHECK failure in
  // FindVisualRectNeedingUpdateScopeBase::CheckVisualRect().
  FrameView().GetLayoutView()->SetSubtreeShouldCheckForPaintInvalidation();

  FrameView().UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

void ValidationMessageOverlayDelegate::CreatePage(const FrameOverlay& overlay) {
  DCHECK(!page_);

  // TODO(tkent): Can we share code with WebPagePopupImpl and
  // InspectorOverlayAgent?
  IntSize view_size = overlay.Size();
  Page::PageClients page_clients;
  FillWithEmptyClients(page_clients);
  chrome_client_ = MakeGarbageCollected<ValidationMessageChromeClient>(
      main_page_->GetChromeClient(), anchor_->GetDocument().View());
  page_clients.chrome_client = chrome_client_;
  Settings& main_settings = main_page_->GetSettings();
  page_ = Page::CreateNonOrdinary(page_clients);
  page_->GetSettings().SetMinimumFontSize(main_settings.GetMinimumFontSize());
  page_->GetSettings().SetMinimumLogicalFontSize(
      main_settings.GetMinimumLogicalFontSize());

  auto* frame = MakeGarbageCollected<LocalFrame>(
      MakeGarbageCollected<EmptyLocalFrameClient>(), *page_, nullptr, nullptr,
      nullptr);
  frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame, view_size));
  frame->Init();
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);
  page_->GetVisualViewport().SetSize(view_size);

  scoped_refptr<SharedBuffer> data = SharedBuffer::Create();
  WriteDocument(data.get());
  float zoom_factor = anchor_->GetDocument().GetFrame()->PageZoomFactor();
  frame->SetPageZoomFactor(zoom_factor);
  // Propagate deprecated DSF for platforms without use-zoom-for-dsf.
  page_->SetDeviceScaleFactorDeprecated(
      main_page_->DeviceScaleFactorDeprecated());
  frame->ForceSynchronousDocumentInstall("text/html", data);

  Element& main_message = GetElementById("main-message");
  main_message.setTextContent(message_);
  Element& sub_message = GetElementById("sub-message");
  sub_message.setTextContent(sub_message_);

  Element& container = GetElementById("container");
  if (WebTestSupport::IsRunningWebTest()) {
    container.SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
    GetElementById("icon").SetInlineStyleProperty(CSSPropertyID::kTransition,
                                                  "none");
    main_message.SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
    sub_message.SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
  }
  // Get the size to decide position later.
  // TODO(schenney): This says get size, so we only need to update to layout.
  FrameView().UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kOther);
  bubble_size_ = container.VisibleBoundsInVisualViewport().Size();
  // Add one because the content sometimes exceeds the exact width due to
  // rounding errors.
  bubble_size_.Expand(1, 0);
  container.SetInlineStyleProperty(CSSPropertyID::kMinWidth,
                                   bubble_size_.Width() / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  container.setAttribute(html_names::kClassAttr, "shown-initially");
  FrameView().UpdateAllLifecyclePhases(
      DocumentLifecycle::LifecycleUpdateReason::kOther);
}

void ValidationMessageOverlayDelegate::WriteDocument(SharedBuffer* data) {
  DCHECK(data);
  PagePopupClient::AddString("<!DOCTYPE html><html><head><style>", data);
  data->Append(UncompressResourceAsBinary(IDR_VALIDATION_BUBBLE_CSS));
  PagePopupClient::AddString("</style></head>", data);
  PagePopupClient::AddString(
      Locale::DefaultLocale().IsRTL() ? "<body dir=rtl>" : "<body dir=ltr>",
      data);
  PagePopupClient::AddString(
      "<div id=container>"
      "<div id=outer-arrow-top></div>"
      "<div id=inner-arrow-top></div>"
      "<div id=spacer-top></div>"
      "<main id=bubble-body>",
      data);
  data->Append(UncompressResourceAsBinary(IDR_VALIDATION_BUBBLE_ICON));
  PagePopupClient::AddString(message_dir_ == TextDirection::kLtr
                                 ? "<div dir=ltr id=main-message></div>"
                                 : "<div dir=rtl id=main-message></div>",
                             data);
  PagePopupClient::AddString(sub_message_dir_ == TextDirection::kLtr
                                 ? "<div dir=ltr id=sub-message></div>"
                                 : "<div dir=rtl id=sub-message></div>",
                             data);
  PagePopupClient::AddString(
      "</main>"
      "<div id=outer-arrow-bottom></div>"
      "<div id=inner-arrow-bottom></div>"
      "<div id=spacer-bottom></div>"
      "</div></body></html>\n",
      data);
}

Element& ValidationMessageOverlayDelegate::GetElementById(
    const AtomicString& id) const {
  Element* element =
      To<LocalFrame>(page_->MainFrame())->GetDocument()->getElementById(id);
  DCHECK(element) << "No element with id=" << id
                  << ". Failed to load the document?";
  return *element;
}

void ValidationMessageOverlayDelegate::AdjustBubblePosition(
    const IntRect& view_rect) {
  if (IsHiding())
    return;
  float zoom_factor = To<LocalFrame>(page_->MainFrame())->PageZoomFactor();
  IntRect anchor_rect = anchor_->VisibleBoundsInVisualViewport();
  bool show_bottom_arrow = false;
  double bubble_y = anchor_rect.MaxY();
  if (view_rect.MaxY() - anchor_rect.MaxY() < bubble_size_.Height()) {
    bubble_y = anchor_rect.Y() - bubble_size_.Height();
    show_bottom_arrow = true;
  }
  double bubble_x =
      anchor_rect.X() + anchor_rect.Width() / 2 - bubble_size_.Width() / 2;
  if (bubble_x < view_rect.X())
    bubble_x = view_rect.X();
  else if (bubble_x + bubble_size_.Width() > view_rect.MaxX())
    bubble_x = view_rect.MaxX() - bubble_size_.Width();

  Element& container = GetElementById("container");
  container.SetInlineStyleProperty(CSSPropertyID::kLeft, bubble_x / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  container.SetInlineStyleProperty(CSSPropertyID::kTop, bubble_y / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);

  // Should match to --arrow-size in validation_bubble.css.
  const int kArrowSize = 8;
  const int kArrowMargin = 10;
  const int kMinArrowAnchorX = kArrowSize + kArrowMargin;
  double max_arrow_anchor_x =
      bubble_size_.Width() - (kArrowSize + kArrowMargin) * zoom_factor;
  double arrow_anchor_x;
  const int kOffsetToAnchorRect = 8;
  double anchor_rect_center = anchor_rect.X() + anchor_rect.Width() / 2;
  if (!Locale::DefaultLocale().IsRTL()) {
    double anchor_rect_left =
        anchor_rect.X() + kOffsetToAnchorRect * zoom_factor;
    if (anchor_rect_left > anchor_rect_center)
      anchor_rect_left = anchor_rect_center;

    arrow_anchor_x = kMinArrowAnchorX * zoom_factor;
    if (bubble_x + arrow_anchor_x < anchor_rect_left) {
      arrow_anchor_x = anchor_rect_left - bubble_x;
      if (arrow_anchor_x > max_arrow_anchor_x)
        arrow_anchor_x = max_arrow_anchor_x;
    }
  } else {
    double anchor_rect_right =
        anchor_rect.MaxX() - kOffsetToAnchorRect * zoom_factor;
    if (anchor_rect_right < anchor_rect_center)
      anchor_rect_right = anchor_rect_center;

    arrow_anchor_x = max_arrow_anchor_x;
    if (bubble_x + arrow_anchor_x > anchor_rect_right) {
      arrow_anchor_x = anchor_rect_right - bubble_x;
      if (arrow_anchor_x < kMinArrowAnchorX * zoom_factor)
        arrow_anchor_x = kMinArrowAnchorX * zoom_factor;
    }
  }
  double arrow_x = arrow_anchor_x / zoom_factor - kArrowSize;
  double arrow_anchor_percent = arrow_anchor_x * 100 / bubble_size_.Width();
  if (show_bottom_arrow) {
    GetElementById("outer-arrow-bottom")
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    GetElementById("inner-arrow-bottom")
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    container.setAttribute(html_names::kClassAttr, "shown-fully bottom-arrow");
    container.SetInlineStyleProperty(
        CSSPropertyID::kTransformOrigin,
        String::Format("%.2f%% bottom", arrow_anchor_percent));
  } else {
    GetElementById("outer-arrow-top")
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    GetElementById("inner-arrow-top")
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    container.setAttribute(html_names::kClassAttr, "shown-fully");
    container.SetInlineStyleProperty(
        CSSPropertyID::kTransformOrigin,
        String::Format("%.2f%% top", arrow_anchor_percent));
  }
}

void ValidationMessageOverlayDelegate::StartToHide() {
  anchor_ = nullptr;
  if (!page_)
    return;
  GetElementById("container")
      .classList()
      .replace("shown-fully", "hiding", ASSERT_NO_EXCEPTION);
}

bool ValidationMessageOverlayDelegate::IsHiding() const {
  return !anchor_;
}

}  // namespace blink
