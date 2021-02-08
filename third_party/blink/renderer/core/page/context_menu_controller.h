/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CONTEXT_MENU_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CONTEXT_MENU_CONTROLLER_H_

#include <memory>
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"

namespace blink {

class ContextMenuProvider;
class Document;
class LocalFrame;
class MouseEvent;
class Page;
struct ContextMenuData;

class CORE_EXPORT ContextMenuController
    : public GarbageCollected<ContextMenuController>,
      public mojom::blink::ContextMenuClient {
 public:
  explicit ContextMenuController(Page*);
  ~ContextMenuController() override;
  void Trace(Visitor*) const;

  void ClearContextMenu();

  void DocumentDetached(Document*);

  void HandleContextMenuEvent(MouseEvent*);
  void ShowContextMenuAtPoint(LocalFrame*,
                              float x,
                              float y,
                              ContextMenuProvider*);

  void CustomContextMenuItemSelected(unsigned action);

  Node* ContextMenuNodeForFrame(LocalFrame*);

  // mojom::blink::ContextMenuClient methods.
  void CustomContextMenuAction(uint32_t action) override;
  void ContextMenuClosed(const KURL& link_followed) override;

 private:
  friend class ContextMenuControllerTest;

  // Returns whether a Context Menu was actually shown.
  bool ShowContextMenu(LocalFrame*,
                       const PhysicalOffset&,
                       WebMenuSourceType,
                       const MouseEvent* mouse_event = nullptr);
  bool ShouldShowContextMenuFromTouch(const ContextMenuData&);

  void UpdateTextFragmentSelectorGenerator(LocalFrame*);

  HeapMojoAssociatedReceiver<mojom::blink::ContextMenuClient,
                             ContextMenuController,
                             HeapMojoWrapperMode::kWithoutContextObserver>
      context_menu_client_receiver_{this, nullptr};

  Member<Page> page_;
  Member<ContextMenuProvider> menu_provider_;
  HitTestResult hit_test_result_;
  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_CONTEXT_MENU_CONTROLLER_H_
