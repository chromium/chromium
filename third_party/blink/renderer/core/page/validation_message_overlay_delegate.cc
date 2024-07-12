// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/validation_message_overlay_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/resources/grit/blink_resources.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_animator.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/platform/data_resource_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/cull_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

// ChromeClient for an internal page of ValidationMessageOverlayDelegate.
class ValidationMessageChromeClient : public EmptyChromeClient {
 public:
  explicit ValidationMessageChromeClient(ChromeClient& main_chrome_client,
                                         LocalFrameView* anchor_view)
      : main_chrome_client_(main_chrome_client), anchor_view_(anchor_view) {}

  void Trace(Visitor* visitor) const override {
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
    anchor_view_->SetVisualViewportOrOverlayNeedsRepaint();
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
  if (destroyed_ptr_)
    *destroyed_ptr_ = true;
}

LocalFrameView& ValidationMessageOverlayDelegate::FrameView() const {
  DCHECK(page_)
      << "Do not call FrameView() before the first call of CreatePage()";
  return *To<LocalFrame>(page_->MainFrame())->View();
}

void ValidationMessageOverlayDelegate::PaintFrameOverlay(
    const FrameOverlay& overlay,
    GraphicsContext& context,
    const gfx::Size& view_size) const {
  if (IsHiding() && !page_)
    return;

  if (DrawingRecorder::UseCachedDrawingIfPossible(context, overlay,
                                                  DisplayItem::kFrameOverlay))
    return;
  DrawingRecorder recorder(context, overlay, DisplayItem::kFrameOverlay,
                           gfx::Rect(view_size));
  context.DrawRecord(FrameView().GetPaintRecord());
}

void ValidationMessageOverlayDelegate::ServiceScriptedAnimations(
    base::TimeTicks monotonic_frame_begin_time) {
  page_->Animator().ServiceScriptedAnimations(monotonic_frame_begin_time);
}

void ValidationMessageOverlayDelegate::UpdateFrameViewState(
    const FrameOverlay& overlay) {
  gfx::Size view_size = overlay.Size();
  if (FrameView().Size() != view_size) {
    FrameView().Resize(view_size);
    page_->GetVisualViewport().SetSize(view_size);
  }
  gfx::Rect intersection = overlay.Frame().RemoteViewportIntersection();
  AdjustBubblePosition(intersection.IsEmpty()
                           ? gfx::Rect(gfx::Point(), view_size)
                           : intersection);

  // This manual invalidation is necessary to avoid a DCHECK failure in
  // FindVisualRectNeedingUpdateScopeBase::CheckVisualRect().
  FrameView().GetLayoutView()->SetSubtreeShouldCheckForPaintInvalidation();

  FrameView().UpdateAllLifecyclePhases(DocumentUpdateReason::kOverlay);
}

void ValidationMessageOverlayDelegate::CreatePage(const FrameOverlay& overlay) {
  DCHECK(!page_);

  // TODO(tkent): Can we share code with WebPagePopupImpl and
  // InspectorOverlayAgent?
  gfx::Size view_size = overlay.Size();
  chrome_client_ = MakeGarbageCollected<ValidationMessageChromeClient>(
      main_page_->GetChromeClient(), anchor_->GetDocument().View());
  Settings& main_settings = main_page_->GetSettings();
  page_ = Page::CreateNonOrdinary(
      *chrome_client_, main_page_->GetPageScheduler()->GetAgentGroupScheduler(),
      &main_page_->GetColorProviderColorMaps());
  page_->GetSettings().SetMinimumFontSize(main_settings.GetMinimumFontSize());
  page_->GetSettings().SetMinimumLogicalFontSize(
      main_settings.GetMinimumLogicalFontSize());

  auto* frame = MakeGarbageCollected<LocalFrame>(
      MakeGarbageCollected<EmptyLocalFrameClient>(), *page_, nullptr, nullptr,
      nullptr, FrameInsertType::kInsertInConstructor, LocalFrameToken(),
      nullptr, nullptr, mojo::NullRemote());
  frame->SetView(MakeGarbageCollected<LocalFrameView>(*frame, view_size));
  frame->Init(/*opener=*/nullptr, DocumentToken(), /*policy_container=*/nullptr,
              StorageKey(), /*document_ukm_source_id=*/ukm::kInvalidSourceId,
              /*creator_base_url=*/KURL());
  frame->View()->SetCanHaveScrollbars(false);
  frame->View()->SetBaseBackgroundColor(Color::kTransparent);
  page_->GetVisualViewport().SetSize(view_size);

  // Propagate dark mode settings from anchor document to allow CSS of
  // overlay bubble to detect dark mode. See the comments in
  // PagePopupClient::AdjustSettingsFromOwnerColorScheme for more information.
  page_->GetSettings().SetForceDarkModeEnabled(false);
  bool in_forced_colors_mode = anchor_->GetDocument().InForcedColorsMode();
  LayoutObject* anchor_layout = anchor_->GetLayoutObject();
  page_->GetSettings().SetPreferredColorScheme(
      !in_forced_colors_mode && anchor_layout &&
              anchor_layout->StyleRef().UsedColorScheme() ==
                  mojom::blink::ColorScheme::kDark
          ? mojom::blink::PreferredColorScheme::kDark
          : mojom::blink::PreferredColorScheme::kLight);

  SegmentedBuffer data;
  WriteDocument(data);
  float zoom_factor = anchor_->GetDocument().GetFrame()->LayoutZoomFactor();
  frame->SetLayoutZoomFactor(zoom_factor);

  // ForceSynchronousDocumentInstall can cause another call to
  // ValidationMessageClientImpl::ShowValidationMessage, which will hide this
  // validation message and may even delete this. In order to avoid continuing
  // when this is destroyed, |destroyed| will be set to true in the destructor.
  bool destroyed = false;
  DCHECK(!destroyed_ptr_);
  destroyed_ptr_ = &destroyed;
  frame->ForceSynchronousDocumentInstall(AtomicString("text/html"),
                                         std::move(data));
  if (destroyed)
    return;
  destroyed_ptr_ = nullptr;

  Element& main_message = GetElementById(AtomicString("main-message"));
  main_message.setTextContent(message_);
  Element& sub_message = GetElementById(AtomicString("sub-message"));
  sub_message.setTextContent(sub_message_);

  Element& container = GetElementById(AtomicString("container"));
  if (WebTestSupport::IsRunningWebTest()) {
    container.SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
    GetElementById(AtomicString("icon"))
        .SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
    main_message.SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
    sub_message.SetInlineStyleProperty(CSSPropertyID::kTransition, "none");
  }
  // Get the size to decide position later.
  // TODO(rendering-core): This gets a size, so we should only need to update
  // to layout.
  FrameView().UpdateAllLifecyclePhases(DocumentUpdateReason::kOverlay);
  bubble_size_ = container.VisibleBoundsInLocalRoot().size();
  // Add one because the content sometimes exceeds the exact width due to
  // rounding errors.
  bubble_size_.Enlarge(1, 0);
  container.SetInlineStyleProperty(CSSPropertyID::kMinWidth,
                                   bubble_size_.width() / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  container.setAttribute(html_names::kClassAttr,
                         AtomicString("shown-initially"));
  FrameView().UpdateAllLifecyclePhases(DocumentUpdateReason::kOverlay);
}

void ValidationMessageOverlayDelegate::WriteDocument(SegmentedBuffer& data) {
  PagePopupClient::AddString(
      "<!DOCTYPE html><head><meta charset='UTF-8'><meta name='color-scheme' "
      "content='light dark'><style>",
      data);
  data.Append(UncompressResourceAsBinary(IDR_VALIDATION_BUBBLE_CSS));
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
  data.Append(UncompressResourceAsBinary(IDR_VALIDATION_BUBBLE_ICON));
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
    const gfx::Rect& view_rect) {
  if (IsHiding())
    return;
  float zoom_factor = To<LocalFrame>(page_->MainFrame())->LayoutZoomFactor();
  gfx::Rect anchor_rect = anchor_->VisibleBoundsInLocalRoot();

  Page* anchor_page = anchor_->GetDocument().GetPage();
  // If the main frame is local the overlay is attached to it so we have to
  // account for the anchor's position relative to the visual viewport. If the
  // main frame is remote the overlay will be attached to the local root so the
  // visual viewport transform will already be applied to the overlay.
  if (IsA<LocalFrame>(anchor_page->MainFrame())) {
    PhysicalRect rect(anchor_rect);
    anchor_->GetDocument()
        .GetFrame()
        ->LocalFrameRoot()
        .ContentLayoutObject()
        ->MapToVisualRectInAncestorSpace(nullptr, rect);
    anchor_rect = ToPixelSnappedRect(rect);
    anchor_rect =
        anchor_page->GetVisualViewport().RootFrameToViewport(anchor_rect);
    anchor_rect.Intersect(gfx::Rect(anchor_page->GetVisualViewport().Size()));
  }

  bool show_bottom_arrow = false;
  double bubble_y = anchor_rect.bottom();
  if (view_rect.bottom() - anchor_rect.bottom() < bubble_size_.height()) {
    bubble_y = anchor_rect.y() - bubble_size_.height();
    show_bottom_arrow = true;
  }
  double bubble_x =
      anchor_rect.x() + anchor_rect.width() / 2 - bubble_size_.width() / 2;
  if (bubble_x < view_rect.x())
    bubble_x = view_rect.x();
  else if (bubble_x + bubble_size_.width() > view_rect.right())
    bubble_x = view_rect.right() - bubble_size_.width();

  Element& container = GetElementById(AtomicString("container"));
  container.SetInlineStyleProperty(CSSPropertyID::kLeft, bubble_x / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);
  container.SetInlineStyleProperty(CSSPropertyID::kTop, bubble_y / zoom_factor,
                                   CSSPrimitiveValue::UnitType::kPixels);

  // Should match to --arrow-size in validation_bubble.css.
  const int kArrowSize = 8;
  const int kArrowMargin = 10;
  const int kMinArrowAnchorX = kArrowSize + kArrowMargin;
  double max_arrow_anchor_x =
      bubble_size_.width() - (kArrowSize + kArrowMargin) * zoom_factor;
  double arrow_anchor_x;
  const int kOffsetToAnchorRect = 8;
  double anchor_rect_center = anchor_rect.x() + anchor_rect.width() / 2;
  if (!Locale::DefaultLocale().IsRTL()) {
    double anchor_rect_left =
        anchor_rect.x() + kOffsetToAnchorRect * zoom_factor;
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
        anchor_rect.right() - kOffsetToAnchorRect * zoom_factor;
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
  double arrow_anchor_percent = arrow_anchor_x * 100 / bubble_size_.width();
  if (show_bottom_arrow) {
    GetElementById(AtomicString("outer-arrow-bottom"))
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    GetElementById(AtomicString("inner-arrow-bottom"))
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    container.setAttribute(html_names::kClassAttr,
                           AtomicString("shown-fully bottom-arrow"));
    container.SetInlineStyleProperty(
        CSSPropertyID::kTransformOrigin,
        String::Format("%.2f%% bottom", arrow_anchor_percent));
  } else {
    GetElementById(AtomicString("outer-arrow-top"))
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    GetElementById(AtomicString("inner-arrow-top"))
        .SetInlineStyleProperty(CSSPropertyID::kLeft, arrow_x,
                                CSSPrimitiveValue::UnitType::kPixels);
    container.setAttribute(html_names::kClassAttr, AtomicString("shown-fully"));
    container.SetInlineStyleProperty(
        CSSPropertyID::kTransformOrigin,
        String::Format("%.2f%% top", arrow_anchor_percent));
  }
}

void ValidationMessageOverlayDelegate::StartToHide() {
  anchor_ = nullptr;
  if (!page_)
    return;
  GetElementById(AtomicString("container"))
      .classList()
      .replace(AtomicString("shown-fully"), AtomicString("hiding"),
               ASSERT_NO_EXCEPTION);
}

bool ValidationMessageOverlayDelegate::IsHiding() const {
  return !anchor_;
}

}  // namespace blink
