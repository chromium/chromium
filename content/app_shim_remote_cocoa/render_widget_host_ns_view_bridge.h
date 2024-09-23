// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_

#include <utility>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#import "content/app_shim_remote_cocoa/popup_window_mac.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#import "content/app_shim_remote_cocoa/sharing_service_picker.h"
#include "content/app_shim_remote_cocoa/sharing_service_picker.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "content/public/common/widget_type.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/base/mojom/attributed_string.mojom-forward.h"
#include "ui/display/display_observer.h"

namespace remote_cocoa {

// Mojo bridge for a RenderWidgetHostViewMac's NSView. This class may be
// instantiated in the same process as its RenderWidgetHostViewMac, or it may
// be in a different process.
class RenderWidgetHostNSViewBridge : public mojom::RenderWidgetHostNSView,
                                     public display::DisplayObserver {
 public:
  RenderWidgetHostNSViewBridge(mojom::RenderWidgetHostNSViewHost* client,
                               RenderWidgetHostNSViewHostHelper* client_helper,
                               uint64_t ns_view_id,
                               base::OnceClosure destroy_callback = {});

  RenderWidgetHostNSViewBridge(const RenderWidgetHostNSViewBridge&) = delete;
  RenderWidgetHostNSViewBridge& operator=(const RenderWidgetHostNSViewBridge&) =
      delete;

  ~RenderWidgetHostNSViewBridge() override;

  // Bind to a remote receiver for a mojo interface.
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::RenderWidgetHostNSView>
          bridge_receiver);

  // TODO(ccameron): RenderWidgetHostViewMac and other functions currently use
  // this method to communicate directly with RenderWidgetHostViewCocoa. The
  // goal of this class is to eliminate this direct communication (so this
  // method is expected to go away).
  RenderWidgetHostViewCocoa* GetNSView();

  // mojom::RenderWidgetHostNSView implementation.
  void InitAsPopup(const gfx::Rect& content_rect,
                   uint64_t popup_parent_ns_view_id) override;
  void SetParentWebContentsNSView(uint64_t parent_ns_view_id) override;
  void DisableDisplay() override;
  void MakeFirstResponder() override;
  void SetBounds(const gfx::Rect& rect) override;
  void SetCALayerParams(const gfx::CALayerParams& ca_layer_params) override;
  void SetBackgroundColor(SkColor color) override;
  void SetVisible(bool visible) override;
  void SetTooltipText(const std::u16string& display_text) override;
  void SetTextInputState(ui::TextInputType text_input_type,
                         uint32_t flags) override;
  void SetTextSelection(const std::u16string& text,
                        uint64_t offset,
                        const gfx::Range& range) override;
  void SetCompositionRangeInfo(const gfx::Range& range) override;
  void CancelComposition() override;
  void SetShowingContextMenu(bool showing) override;
  void DisplayCursor(const ui::Cursor& cursor) override;
  void SetCursorLocked(bool locked) override;
  void SetCursorLockedUnacceleratedMovement(bool unaccelerated) override;
  void ShowDictionaryOverlayForSelection() override;
  void ShowDictionaryOverlay(ui::mojom::AttributedStringPtr attributed_string,
                             const gfx::Point& baseline_point) override;
  void LockKeyboard(
      const std::optional<std::vector<uint32_t>>& uint_dom_codes) override;
  void UnlockKeyboard() override;
  void ShowSharingServicePicker(
      const std::string& title,
      const std::string& text,
      const std::string& url,
      const std::vector<std::string>& file_paths,
      ShowSharingServicePickerCallback callback) override;
  void Destroy() override;
  void GestureScrollEventAck(
      std::unique_ptr<blink::WebCoalescedInputEvent> event,
      bool consumed) override;
  void DidOverscroll(blink::mojom::DidOverscrollParamsPtr params) override;
  void DisplayPopupMenu(mojom::PopupMenuPtr menu,
                        DisplayPopupMenuCallback callback) override;

 private:
  bool IsPopup() const { return !!popup_window_; }

  // display::DisplayObserver implementation.
  void OnDisplayAdded(const display::Display&) override;
  void OnDisplaysRemoved(const display::Displays&) override;
  void OnDisplayMetricsChanged(const display::Display&, uint32_t) override;

  void OnSharingServiceInvoked(ShowSharingServicePickerCallback callback,
                               blink::mojom::ShareError error);

  // The NSView used for input and display.
  RenderWidgetHostViewCocoa* __strong cocoa_view_;

  // NSSharingServicePicker for the navigator.share API.
  SharingServicePicker* __strong sharing_service_picker_;

  // Once set, all calls to set the background color or CALayer content will
  // be ignored.
  bool display_disabled_ = false;

  // The window used for popup widgets, and its helper.
  std::unique_ptr<PopupWindowMac> popup_window_;

  // The background CALayer which is hosted by |cocoa_view_|, and is used as
  // the root of |display_ca_layer_tree_|.
  CALayer* __strong background_layer_;
  std::unique_ptr<ui::DisplayCALayerTree> display_ca_layer_tree_;

  // Cached copy of the tooltip text, to avoid redundant calls.
  std::u16string tooltip_text_;

  display::ScopedDisplayObserver display_observer_{this};

  std::unique_ptr<ScopedNSViewIdMapping> view_id_;

  // The receiver for this object (only used when remotely instantiated).
  mojo::AssociatedReceiver<mojom::RenderWidgetHostNSView> receiver_{this};

  // The callback to be called when `Destroy()` is called.
  base::OnceClosure destroy_callback_;

  // A DisplayPopupMenu call might come in while we're still displaying a popup
  // menu. As at that point we're in a nested run loop, we'll need to delay
  // displaying the menu until the nested look has finished. To accomplish this
  // we keep track of pending DisplayPopupMenu calls.
  using PendingPopupMenu =
      std::pair<mojom::PopupMenuPtr, DisplayPopupMenuCallback>;
  std::vector<PendingPopupMenu> pending_menus_;
  bool showing_popup_menu_ = false;

  base::WeakPtrFactory<RenderWidgetHostNSViewBridge> weak_factory_{this};
};

}  // namespace remote_cocoa

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
