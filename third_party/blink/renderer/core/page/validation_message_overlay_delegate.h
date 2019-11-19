// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_VALIDATION_MESSAGE_OVERLAY_DELEGATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_VALIDATION_MESSAGE_OVERLAY_DELEGATE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame_overlay.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ChromeClient;
class Element;
class LocalFrameView;
class Page;

// A ValidationMessageOverlayDelegate is responsible for rendering a form
// validation message bubble.
//
// Lifetime: An instance is created by a ValidationMessageClientImpl when a
// bubble is shown, and deleted when the bubble is closed.
//
// Ownership: A FrameOverlay instance owns a ValidationMessageOverlayDelegate.
class CORE_EXPORT ValidationMessageOverlayDelegate
    : public FrameOverlay::Delegate {
 public:
  ValidationMessageOverlayDelegate(Page& main_page,
                                   const Element& anchor,
                                   const String& message,
                                   TextDirection message_dir,
                                   const String& sub_message,
                                   TextDirection sub_message_dir);
  ~ValidationMessageOverlayDelegate() override;

  void CreatePage(const FrameOverlay&);

  // FrameOverlay::Delegate implementation.
  void PaintFrameOverlay(const FrameOverlay&,
                         GraphicsContext&,
                         const IntSize& view_size) const override;
  void ServiceScriptedAnimations(base::TimeTicks) override;

  void StartToHide();
  bool IsHiding() const;

  Page* GetPageForTesting() const { return page_; }

 private:
  LocalFrameView& FrameView() const;
  void UpdateFrameViewState(const FrameOverlay&, const IntSize& view_size);
  void WriteDocument(SharedBuffer*);
  Element& GetElementById(const AtomicString&) const;
  void AdjustBubblePosition(const IntRect& view_rect);

  // An internal Page and a ChromeClient for it.
  Persistent<Page> page_;
  Persistent<ChromeClient> chrome_client_;

  IntSize bubble_size_;

  // A page which triggered this validation message.
  Persistent<Page> main_page_;

  Persistent<const Element> anchor_;
  String message_;
  String sub_message_;
  TextDirection message_dir_;
  TextDirection sub_message_dir_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_VALIDATION_MESSAGE_OVERLAY_DELEGATE_H_
