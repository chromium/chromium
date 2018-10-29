/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple, Inc. All rights
 * reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CHROME_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CHROME_CLIENT_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "cc/input/event_listener_properties.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/platform/blame_context.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/inspector/console_types.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/cursor.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef CreateWindow

namespace cc {
class Layer;
struct OverscrollBehavior;
}

namespace blink {

class ColorChooser;
class ColorChooserClient;
class CompositorAnimationTimeline;
class DateTimeChooser;
class DateTimeChooserClient;
class Element;
class FileChooser;
class FloatPoint;
class Frame;
class FullscreenOptions;
class GraphicsLayer;
class HTMLFormControlElement;
class HTMLInputElement;
class HTMLSelectElement;
class HitTestLocation;
class HitTestResult;
class IntRect;
class KeyboardEvent;
class LocalFrame;
class LocalFrameView;
class Node;
class Page;
class PagePopup;
class PagePopupClient;
class PopupOpeningObserver;
class WebDragData;
class WebLayerTreeView;
class WebViewImpl;

struct DateTimeChooserParameters;
struct FrameLoadRequest;
struct ViewportDescription;
struct WebCursorInfo;
struct WebPoint;
struct WebScreenInfo;
struct WebWindowFeatures;

class CORE_EXPORT ChromeClient
    : public GarbageCollectedFinalized<ChromeClient> {
  DISALLOW_COPY_AND_ASSIGN(ChromeClient);

 public:
  virtual ~ChromeClient() = default;

  // Converts the scalar value from the window coordinates to the viewport
  // scale.
  virtual float WindowToViewportScalar(const float) const = 0;

  virtual bool IsPopup() { return false; }

  virtual void ChromeDestroyed() = 0;

  // Requests the host invalidate the contents.
  virtual void InvalidateRect(const IntRect& update_rect) = 0;

  // Converts the rect from the viewport coordinates to screen coordinates.
  virtual IntRect ViewportToScreen(const IntRect&,
                                   const LocalFrameView*) const = 0;

  virtual void ScheduleAnimation(const LocalFrameView*) = 0;

  // The specified rectangle is adjusted for the minimum window size and the
  // screen, then setWindowRect with the adjusted rectangle is called.
  void SetWindowRectWithAdjustment(const IntRect&, LocalFrame&);
  virtual IntRect RootWindowRect() = 0;

  virtual IntRect PageRect() = 0;

  virtual void Focus(LocalFrame*) = 0;

  virtual bool CanTakeFocus(WebFocusType) = 0;
  virtual void TakeFocus(WebFocusType) = 0;

  virtual void FocusedNodeChanged(Node*, Node*) = 0;

  virtual bool HadFormInteraction() const = 0;

  virtual void BeginLifecycleUpdates() = 0;

  // Start a system drag and drop operation.
  virtual void StartDragging(LocalFrame*,
                             const WebDragData&,
                             WebDragOperationsMask,
                             const SkBitmap& drag_image,
                             const WebPoint& drag_image_offset) = 0;
  virtual bool AcceptsLoadDrops() const = 0;

  // The LocalFrame pointer provides the ChromeClient with context about which
  // LocalFrame wants to create the new Page. Also, the newly created window
  // should not be shown to the user until the ChromeClient of the newly
  // created Page has its show method called.
  // The FrameLoadRequest parameter is only for ChromeClient to check if the
  // request could be fulfilled. The ChromeClient should not load the request.
  virtual Page* CreateWindow(LocalFrame*,
                             const FrameLoadRequest&,
                             const WebWindowFeatures&,
                             NavigationPolicy,
                             SandboxFlags,
                             const SessionStorageNamespaceId&) = 0;
  virtual void Show(NavigationPolicy) = 0;

  // All the parameters should be in viewport space. That is, if an event
  // scrolls by 10 px, but due to a 2X page scale we apply a 5px scroll to the
  // root frame, all of which is handled as overscroll, we should return 10px
  // as the overscrollDelta.
  virtual void DidOverscroll(const FloatSize& overscroll_delta,
                             const FloatSize& accumulated_overscroll,
                             const FloatPoint& position_in_viewport,
                             const FloatSize& velocity_in_viewport,
                             const cc::OverscrollBehavior&) = 0;

  virtual bool ShouldReportDetailedMessageForSource(LocalFrame&,
                                                    const String& source) = 0;
  virtual void AddMessageToConsole(LocalFrame*,
                                   MessageSource,
                                   MessageLevel,
                                   const String& message,
                                   unsigned line_number,
                                   const String& source_id,
                                   const String& stack_trace) = 0;

  virtual bool CanOpenBeforeUnloadConfirmPanel() = 0;
  bool OpenBeforeUnloadConfirmPanel(const String& message,
                                    LocalFrame*,
                                    bool is_reload);

  virtual void CloseWindowSoon() = 0;

  bool OpenJavaScriptAlert(LocalFrame*, const String&);
  bool OpenJavaScriptConfirm(LocalFrame*, const String&);
  bool OpenJavaScriptPrompt(LocalFrame*,
                            const String& message,
                            const String& default_value,
                            String& result);
  virtual bool TabsToLinks() = 0;

  virtual WebViewImpl* GetWebView() const = 0;

  virtual WebScreenInfo GetScreenInfo() const = 0;
  virtual void SetCursor(const Cursor&, LocalFrame* local_root) = 0;

  virtual void SetCursorOverridden(bool) = 0;

  virtual void AutoscrollStart(WebFloatPoint position, LocalFrame*) {}
  virtual void AutoscrollFling(WebFloatSize velocity, LocalFrame*) {}
  virtual void AutoscrollEnd(LocalFrame*) {}

  virtual Cursor LastSetCursorForTesting() const = 0;
  Node* LastSetTooltipNodeForTesting() const {
    return last_mouse_over_node_.Get();
  }

  virtual void SetCursorForPlugin(const WebCursorInfo&, LocalFrame*) = 0;

  // Returns a custom visible content rect if a viewport override is active.
  virtual base::Optional<IntRect> VisibleContentRectForPainting() const {
    return base::nullopt;
  }

  virtual void DispatchViewportPropertiesDidChange(
      const ViewportDescription&) const {}

  virtual void ContentsSizeChanged(LocalFrame*, const IntSize&) const = 0;
  virtual void PageScaleFactorChanged() const {}
  virtual float ClampPageScaleFactorToLimits(float scale) const {
    return scale;
  }
  virtual void MainFrameScrollOffsetChanged() const {}
  virtual void ResizeAfterLayout() const {}
  virtual void MainFrameLayoutUpdated() const {}

  void MouseDidMoveOverElement(LocalFrame&,
                               const HitTestLocation&,
                               const HitTestResult&);
  virtual void SetToolTip(LocalFrame&, const String&, TextDirection) = 0;
  void ClearToolTip(LocalFrame&);

  bool Print(LocalFrame*);

  virtual ColorChooser* OpenColorChooser(LocalFrame*,
                                         ColorChooserClient*,
                                         const Color&) = 0;

  // This function is used for:
  //  - Mandatory date/time choosers if InputMultipleFieldsUI flag is not set
  //  - Date/time choosers for types for which
  //    LayoutTheme::SupportsCalendarPicker returns true, if
  //    InputMultipleFieldsUI flag is set
  //  - <datalist> UI for date/time input types regardless of
  //    InputMultipleFieldsUI flag
  virtual DateTimeChooser* OpenDateTimeChooser(
      DateTimeChooserClient*,
      const DateTimeChooserParameters&) = 0;

  virtual void OpenTextDataListChooser(HTMLInputElement&) = 0;

  virtual void OpenFileChooser(LocalFrame*, scoped_refptr<FileChooser>) = 0;

  // Asychronous request to enumerate all files in a directory chosen by the
  // user.
  virtual void EnumerateChosenDirectory(FileChooser*) = 0;

  // Pass nullptr as the GraphicsLayer to detach the root layer.
  // This sets the graphics layer for the LocalFrame's WebWidget, if it has
  // one. Otherwise it sets it for the WebViewImpl.
  virtual void AttachRootGraphicsLayer(GraphicsLayer*,
                                       LocalFrame* local_root) = 0;

  // Pass nullptr as the cc::Layer to detach the root layer.
  // This sets the cc::Layer for the LocalFrame's WebWidget, if it has
  // one. Otherwise it sets it for the WebViewImpl.
  virtual void AttachRootLayer(scoped_refptr<cc::Layer>,
                               LocalFrame* local_root) = 0;

  virtual void AttachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                                 LocalFrame* local_root) {}
  virtual void DetachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                                 LocalFrame* local_root) {}

  virtual void EnterFullscreen(LocalFrame&, const FullscreenOptions&) {}
  virtual void ExitFullscreen(LocalFrame&) {}
  virtual void FullscreenElementChanged(Element* old_element,
                                        Element* new_element) {}

  virtual void ClearLayerSelection(LocalFrame*) {}
  virtual void UpdateLayerSelection(LocalFrame*, const cc::LayerSelection&) {}

  virtual void SetEventListenerProperties(LocalFrame*,
                                          cc::EventListenerClass,
                                          cc::EventListenerProperties) = 0;
  virtual cc::EventListenerProperties EventListenerProperties(
      LocalFrame*,
      cc::EventListenerClass) const = 0;
  virtual void SetHasScrollEventHandlers(LocalFrame*, bool) = 0;
  virtual void SetNeedsLowLatencyInput(LocalFrame*, bool) = 0;
  virtual void RequestUnbufferedInputEvents(LocalFrame*) = 0;
  virtual void SetTouchAction(LocalFrame*, TouchAction) = 0;

  // Checks if there is an opened popup, called by LayoutMenuList::showPopup().
  virtual bool HasOpenedPopup() const = 0;
  virtual PopupMenu* OpenPopupMenu(LocalFrame&, HTMLSelectElement&) = 0;
  virtual PagePopup* OpenPagePopup(PagePopupClient*) = 0;
  virtual void ClosePagePopup(PagePopup*) = 0;
  virtual DOMWindow* PagePopupWindowForTesting() const = 0;

  virtual void SetBrowserControlsState(float top_height,
                                       float bottom_height,
                                       bool shrinks_layout){};
  virtual void SetBrowserControlsShownRatio(float){};

  virtual String AcceptLanguages() = 0;

  enum DialogType {
    kAlertDialog = 0,
    kConfirmDialog = 1,
    kPromptDialog = 2,
    kHTMLDialog = 3,
    kPrintDialog = 4
  };
  virtual bool ShouldOpenModalDialogDuringPageDismissal(
      LocalFrame&,
      DialogType,
      const String&,
      Document::PageDismissalType) const {
    return true;
  }

  virtual bool IsSVGImageChromeClient() const { return false; }

  virtual bool RequestPointerLock(LocalFrame*) { return false; }
  virtual void RequestPointerUnlock(LocalFrame*) {}

  virtual IntSize MinimumWindowSize() const { return IntSize(100, 100); }

  virtual bool IsChromeClientImpl() const { return false; }

  virtual void DidAssociateFormControlsAfterLoad(LocalFrame*) {}
  virtual void DidChangeValueInTextField(HTMLFormControlElement&) {}
  virtual void DidEndEditingOnTextField(HTMLInputElement&) {}
  virtual void HandleKeyboardEventOnTextField(HTMLInputElement&,
                                              KeyboardEvent&) {}
  virtual void TextFieldDataListChanged(HTMLInputElement&) {}
  virtual void DidChangeSelectionInSelectControl(HTMLFormControlElement&) {}
  virtual void SelectFieldOptionsChanged(HTMLFormControlElement&) {}
  virtual void AjaxSucceeded(LocalFrame*) {}

  // Input method editor related functions.
  virtual void ShowVirtualKeyboardOnElementFocus(LocalFrame&) {}

  virtual void RegisterViewportLayers() const {}

  virtual void OnMouseDown(Node&) {}

  virtual void DidUpdateBrowserControls() const {}

  virtual void SetOverscrollBehavior(const cc::OverscrollBehavior&) {}

  virtual void RegisterPopupOpeningObserver(PopupOpeningObserver*) = 0;
  virtual void UnregisterPopupOpeningObserver(PopupOpeningObserver*) = 0;
  virtual void NotifyPopupOpeningObservers() const = 0;

  virtual FloatSize ElasticOverscroll() const { return FloatSize(); }

  virtual void InstallSupplements(LocalFrame&);

  virtual WebLayerTreeView* GetWebLayerTreeView(LocalFrame*) { return nullptr; }

  virtual void RequestDecode(LocalFrame*,
                             const PaintImage& image,
                             base::OnceCallback<void(bool)> callback) {
    std::move(callback).Run(false);
  }

  virtual void Trace(blink::Visitor*);

 protected:
  ChromeClient() = default;

  virtual void ShowMouseOverURL(const HitTestResult&) = 0;
  virtual void SetWindowRect(const IntRect&, LocalFrame&) = 0;
  virtual bool OpenBeforeUnloadConfirmPanelDelegate(LocalFrame*,
                                                    bool is_reload) = 0;
  virtual bool OpenJavaScriptAlertDelegate(LocalFrame*, const String&) = 0;
  virtual bool OpenJavaScriptConfirmDelegate(LocalFrame*, const String&) = 0;
  virtual bool OpenJavaScriptPromptDelegate(LocalFrame*,
                                            const String& message,
                                            const String& default_value,
                                            String& result) = 0;
  virtual void PrintDelegate(LocalFrame*) = 0;

 private:
  bool CanOpenModalIfDuringPageDismissal(Frame& main_frame,
                                         DialogType,
                                         const String& message);
  void SetToolTip(LocalFrame&, const HitTestLocation&, const HitTestResult&);

  WeakMember<Node> last_mouse_over_node_;
  LayoutPoint last_tool_tip_point_;
  String last_tool_tip_text_;

  FRIEND_TEST_ALL_PREFIXES(ChromeClientTest, SetToolTipFlood);
  FRIEND_TEST_ALL_PREFIXES(ChromeClientTest, SetToolTipEmptyString);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CHROME_CLIENT_H_
