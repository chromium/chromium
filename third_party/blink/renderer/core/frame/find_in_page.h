// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FIND_IN_PAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FIND_IN_PAGE_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class WebLocalFrameImpl;
class WebString;
struct WebFloatRect;

class CORE_EXPORT FindInPage final : public GarbageCollected<FindInPage>,
                                     public mojom::blink::FindInPage {
 public:
  FindInPage(WebLocalFrameImpl& frame, InterfaceRegistry* interface_registry);

  bool FindInternal(int identifier,
                    const WebString& search_text,
                    const mojom::blink::FindOptions&,
                    bool wrap_within_frame,
                    bool* active_now = nullptr);

  void SetTickmarks(const WebVector<WebRect>&);

  int FindMatchMarkersVersion() const;

  // Returns the bounding box of the active find-in-page match marker or an
  // empty rect if no such marker exists. The rect is returned in find-in-page
  // coordinates.
  WebFloatRect ActiveFindMatchRect();

  void ReportFindInPageMatchCount(int request_id, int count, bool final_update);

  void ReportFindInPageSelection(int request_id,
                                 int active_match_ordinal,
                                 const blink::WebRect& selection_rect,
                                 bool final_update);

  // mojom::blink::FindInPage overrides
  void Find(int request_id,
            const String& search_text,
            mojom::blink::FindOptionsPtr) final;

  void SetClient(mojo::PendingRemote<mojom::blink::FindInPageClient>) final;

  void ActivateNearestFindResult(int request_id, const WebFloatPoint&) final;

  // Stops the current find-in-page, following the given |action|
  void StopFinding(mojom::StopFindAction action) final;

  // Returns the distance (squared) to the closest find-in-page match from the
  // provided point, in find-in-page coordinates.
  void GetNearestFindResult(const WebFloatPoint&,
                            GetNearestFindResultCallback) final;

  // Returns the bounding boxes of the find-in-page match markers in the frame,
  // in find-in-page coordinates.
  void FindMatchRects(int current_version, FindMatchRectsCallback) final;

  // Clears the active find match in the frame, if one exists.
  void ClearActiveFindMatch() final;

  TextFinder* GetTextFinder() const;

  // Returns the text finder object if it already exists.
  // Otherwise creates it and then returns.
  TextFinder& EnsureTextFinder();

  void SetPluginFindHandler(WebPluginContainer* plugin);

  WebPluginContainer* PluginFindHandler() const;

  WebPlugin* GetWebPluginForFind();

  void BindToReceiver(
      mojo::PendingAssociatedReceiver<mojom::blink::FindInPage> receiver);

  void Dispose();

  void Trace(blink::Visitor* visitor) {
    visitor->Trace(text_finder_);
    visitor->Trace(frame_);
  }

 private:
  // Will be initialized after first call to ensureTextFinder().
  Member<TextFinder> text_finder_;

  WebPluginContainer* plugin_find_handler_;

  const Member<WebLocalFrameImpl> frame_;

  mojo::Remote<mojom::blink::FindInPageClient> client_;

  mojo::AssociatedReceiver<mojom::blink::FindInPage> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(FindInPage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FIND_IN_PAGE_H_
