// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_OWNER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_OWNER_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scroll/scroll_types.h"

namespace blink {

class Frame;
class ResourceTimingInfo;

// Oilpan: all FrameOwner instances are GCed objects. FrameOwner additionally
// derives from GarbageCollectedMixin so that Member<FrameOwner> references can
// be kept (e.g., Frame::m_owner.)
class CORE_EXPORT FrameOwner : public GarbageCollectedMixin {
 public:
  virtual ~FrameOwner() = default;

  void Trace(blink::Visitor* visitor) override {}

  virtual bool IsLocal() const = 0;
  virtual bool IsRemote() const = 0;
  virtual bool IsPlugin() const { return false; }

  virtual Frame* ContentFrame() const = 0;
  virtual void SetContentFrame(Frame&) = 0;
  virtual void ClearContentFrame() = 0;

  virtual SandboxFlags GetSandboxFlags() const = 0;
  // Note: there is a subtle ordering dependency here: if a page load needs to
  // report resource timing information, it *must* do so before calling
  // DispatchLoad().
  virtual void AddResourceTiming(const ResourceTimingInfo&) = 0;
  virtual void DispatchLoad() = 0;

  // On load failure, a frame can ask its owner to render fallback content
  // which replaces the frame contents.
  virtual bool CanRenderFallbackContent() const = 0;

  // The argument refers to the frame with the failed navigation. Note that this
  // is not always the ContentFrame() for this owner; this argument is needed to
  // support showing fallback using DOM of parent frame in a separate process.
  // The use case is limited to RemoteFrameOwner when the corresponding local
  // FrameOwner in parent process is an <object>. In such cases the frame with
  // failed navigation could be provisional (cross-site navigations).
  virtual void RenderFallbackContent(Frame*) = 0;

  // The intrinsic dimensions of the embedded object changed. This is only
  // relevant for SVG documents that are embedded via <object> or <embed>.
  virtual void IntrinsicSizingInfoChanged() = 0;

  // Returns the 'name' content attribute value of the browsing context
  // container.
  // https://html.spec.whatwg.org/multipage/browsers.html#browsing-context-container
  virtual AtomicString BrowsingContextContainerName() const = 0;
  virtual ScrollbarMode ScrollingMode() const = 0;
  virtual int MarginWidth() const = 0;
  virtual int MarginHeight() const = 0;
  virtual bool AllowFullscreen() const = 0;
  virtual bool AllowPaymentRequest() const = 0;
  virtual bool IsDisplayNone() const = 0;
  virtual AtomicString RequiredCsp() const = 0;
  virtual const ParsedFeaturePolicy& ContainerPolicy() const = 0;

  // Returns whether or not children of the owned frame should be lazily loaded.
  virtual bool ShouldLazyLoadChildren() const = 0;
};

// TODO(dcheng): This class is an internal implementation detail of provisional
// frames. Move this into WebLocalFrameImpl.cpp and remove existing dependencies
// on it.
class CORE_EXPORT DummyFrameOwner final
    : public GarbageCollectedFinalized<DummyFrameOwner>,
      public FrameOwner {
  USING_GARBAGE_COLLECTED_MIXIN(DummyFrameOwner);

 public:
  static DummyFrameOwner* Create() { return new DummyFrameOwner; }

  void Trace(blink::Visitor* visitor) override { FrameOwner::Trace(visitor); }

  // FrameOwner overrides:
  Frame* ContentFrame() const override { return nullptr; }
  void SetContentFrame(Frame&) override {}
  void ClearContentFrame() override {}
  SandboxFlags GetSandboxFlags() const override { return kSandboxNone; }
  void AddResourceTiming(const ResourceTimingInfo&) override {}
  void DispatchLoad() override {}
  bool CanRenderFallbackContent() const override { return false; }
  void RenderFallbackContent(Frame*) override {}
  void IntrinsicSizingInfoChanged() override {}
  AtomicString BrowsingContextContainerName() const override {
    return AtomicString();
  }
  ScrollbarMode ScrollingMode() const override { return kScrollbarAuto; }
  int MarginWidth() const override { return -1; }
  int MarginHeight() const override { return -1; }
  bool AllowFullscreen() const override { return false; }
  bool AllowPaymentRequest() const override { return false; }
  bool IsDisplayNone() const override { return false; }
  AtomicString RequiredCsp() const override { return g_null_atom; }
  const ParsedFeaturePolicy& ContainerPolicy() const override {
    DEFINE_STATIC_LOCAL(ParsedFeaturePolicy, container_policy, ());
    return container_policy;
  }
  bool ShouldLazyLoadChildren() const override { return false; }

 private:
  // Intentionally private to prevent redundant checks when the type is
  // already DummyFrameOwner.
  bool IsLocal() const override { return false; }
  bool IsRemote() const override { return false; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_FRAME_OWNER_H_
