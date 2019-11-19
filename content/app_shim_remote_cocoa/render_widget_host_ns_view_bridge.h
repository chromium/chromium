// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "content/app_shim_remote_cocoa/popup_window_mac.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "content/public/common/widget_type.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/display/display_observer.h"

namespace remote_cocoa {

// Mojo bridge for a RenderWidgetHostViewMac's NSView. This class may be
// instantiated in the same process as its RenderWidgetHostViewMac, or it may
// be in a different process.
class RenderWidgetHostNSViewBridge : public mojom::RenderWidgetHostNSView,
                                     public display::DisplayObserver {
 public:
  RenderWidgetHostNSViewBridge(mojom::RenderWidgetHostNSViewHost* client,
                               RenderWidgetHostNSViewHostHelper* client_helper);
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
  void InitAsPopup(const gfx::Rect& content_rect) override;
  void SetParentWebContentsNSView(uint64_t parent_ns_view_id) override;
  void DisableDisplay() override;
  void MakeFirstResponder() override;
  void SetBounds(const gfx::Rect& rect) override;
  void SetCALayerParams(const gfx::CALayerParams& ca_layer_params) override;
  void SetBackgroundColor(SkColor color) override;
  void SetVisible(bool visible) override;
  void SetTooltipText(const base::string16& display_text) override;
  void SetTextInputState(ui::TextInputType text_input_type,
                         uint32_t flags) override;
  void SetTextSelection(const base::string16& text,
                        uint64_t offset,
                        const gfx::Range& range) override;
  void SetCompositionRangeInfo(const gfx::Range& range) override;
  void CancelComposition() override;
  void SetShowingContextMenu(bool showing) override;
  void DisplayCursor(const content::WebCursor& cursor) override;
  void SetCursorLocked(bool locked) override;
  void ShowDictionaryOverlayForSelection() override;
  void ShowDictionaryOverlay(
      const mac::AttributedStringCoder::EncodedString& encoded_string,
      const gfx::Point& baseline_point) override;
  void LockKeyboard(
      const base::Optional<std::vector<uint32_t>>& uint_dom_codes) override;
  void UnlockKeyboard() override;

 private:
  bool IsPopup() const { return !!popup_window_; }

  // display::DisplayObserver implementation.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // The NSView used for input and display.
  base::scoped_nsobject<RenderWidgetHostViewCocoa> cocoa_view_;

  // Once set, all calls to set the background color or CALayer content will
  // be ignored.
  bool display_disabled_ = false;

  // The window used for popup widgets, and its helper.
  std::unique_ptr<PopupWindowMac> popup_window_;

  // The background CALayer which is hosted by |cocoa_view_|, and is used as
  // the root of |display_ca_layer_tree_|.
  base::scoped_nsobject<CALayer> background_layer_;
  std::unique_ptr<ui::DisplayCALayerTree> display_ca_layer_tree_;

  // Cached copy of the tooltip text, to avoid redundant calls.
  base::string16 tooltip_text_;

  // The receiver for this object (only used when remotely instantiated).
  mojo::AssociatedReceiver<mojom::RenderWidgetHostNSView> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostNSViewBridge);
};

}  // namespace remote_cocoa

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_NS_VIEW_BRIDGE_H_
