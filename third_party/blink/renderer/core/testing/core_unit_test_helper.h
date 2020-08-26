// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_CORE_UNIT_TEST_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_CORE_UNIT_TEST_HELPER_H_

#include <gtest/gtest.h>
#include <memory>

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayer;

class SingleChildLocalFrameClient final : public EmptyLocalFrameClient {
 public:
  explicit SingleChildLocalFrameClient() = default;

  void Trace(Visitor* visitor) const override {
    EmptyLocalFrameClient::Trace(visitor);
  }

  // LocalFrameClient overrides:
  LocalFrame* CreateFrame(const AtomicString& name,
                          HTMLFrameOwnerElement*) override;
};

class LocalFrameClientWithParent final : public EmptyLocalFrameClient {
 public:
  explicit LocalFrameClientWithParent(LocalFrame* parent) : parent_(parent) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(parent_);
    EmptyLocalFrameClient::Trace(visitor);
  }

  // FrameClient overrides:
  void Detached(FrameDetachType) override;

 private:
  Member<LocalFrame> parent_;
};

// RenderingTestChromeClient ensures that we have a LayerTreeHost which allows
// testing property tree creation.
class RenderingTestChromeClient : public EmptyChromeClient {
 public:
  void SetUp() {
    // Runtime flags can affect LayerTreeHost's settings so this needs to be
    // recreated for each test.
    layer_tree_.reset(new LayerTreeHostEmbedder());
    device_emulation_transform_ = TransformationMatrix();
  }

  bool HasLayer(const cc::Layer& layer) {
    return layer.layer_tree_host() == layer_tree_->layer_tree_host();
  }

  void AttachRootLayer(scoped_refptr<cc::Layer> layer,
                       LocalFrame* local_root) override {
    layer_tree_->layer_tree_host()->SetRootLayer(std::move(layer));
  }

  cc::LayerTreeHost* layer_tree_host() {
    return layer_tree_->layer_tree_host();
  }

  void SetDeviceEmulationTransform(const TransformationMatrix& t) {
    device_emulation_transform_ = t;
  }
  TransformationMatrix GetDeviceEmulationTransform() const override {
    return device_emulation_transform_;
  }

  void InjectGestureScrollEvent(LocalFrame& local_frame,
                                WebGestureDevice device,
                                const gfx::Vector2dF& delta,
                                ScrollGranularity granularity,
                                CompositorElementId scrollable_area_element_id,
                                WebInputEvent::Type injected_type) override;

 private:
  std::unique_ptr<LayerTreeHostEmbedder> layer_tree_;
  TransformationMatrix device_emulation_transform_;
};

class RenderingTest : public PageTestBase {
  USING_FAST_MALLOC(RenderingTest);

 public:
  virtual FrameSettingOverrideFunction SettingOverrider() const {
    return nullptr;
  }
  virtual RenderingTestChromeClient& GetChromeClient() const;

  explicit RenderingTest(LocalFrameClient* = nullptr);

  const Node* HitTest(int x, int y);
  HitTestResult::NodeSet RectBasedHitTest(const PhysicalRect& rect);

 protected:
  void SetUp() override;
  void TearDown() override;

  LayoutView& GetLayoutView() const {
    return *GetDocument().View()->GetLayoutView();
  }

  LocalFrame& ChildFrame() {
    return *To<LocalFrame>(GetFrame().Tree().FirstChild());
  }
  Document& ChildDocument() { return *ChildFrame().GetDocument(); }

  void SetChildFrameHTML(const String&);

  void RunDocumentLifecycle() {
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    UpdateAllLifecyclePhasesForTest();
  }

  LayoutObject* GetLayoutObjectByElementId(const char* id) const {
    const auto* element = GetElementById(id);
    return element ? element->GetLayoutObject() : nullptr;
  }

  PaintLayer* GetPaintLayerByElementId(const char* id) {
    return ToLayoutBoxModelObject(GetLayoutObjectByElementId(id))->Layer();
  }

  const DisplayItemClient* GetDisplayItemClientFromLayoutObject(
      LayoutObject* obj) const {
    LayoutNGBlockFlow* block_flow = ToLayoutNGBlockFlowOrNull(obj);
    if (block_flow && block_flow->PaintFragment())
      return block_flow->PaintFragment();
    return obj;
  }

  const DisplayItemClient* GetDisplayItemClientFromElementId(
      const char* id) const {
    return GetDisplayItemClientFromLayoutObject(GetLayoutObjectByElementId(id));
  }

 private:
  Persistent<LocalFrameClient> local_frame_client_;
};

// These constructors are for convenience of tests to construct these geometries
// from integers.
inline LogicalOffset::LogicalOffset(int inline_offset, int block_offset)
    : inline_offset(inline_offset), block_offset(block_offset) {}
inline LogicalSize::LogicalSize(int inline_size, int block_size)
    : inline_size(inline_size), block_size(block_size) {}
inline LogicalRect::LogicalRect(int inline_offset,
                                int block_offset,
                                int inline_size,
                                int block_size)
    : offset(inline_offset, block_offset), size(inline_size, block_size) {}
inline PhysicalOffset::PhysicalOffset(int left, int top)
    : left(left), top(top) {}
inline PhysicalSize::PhysicalSize(int width, int height)
    : width(width), height(height) {}
inline PhysicalRect::PhysicalRect(int left, int top, int width, int height)
    : offset(left, top), size(width, height) {}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_CORE_UNIT_TEST_HELPER_H_
