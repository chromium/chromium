
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SET_H_

#include "base/macros.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/pausable_object.h"
#include "third_party/blink/renderer/platform/async_method_runner.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"

// Mac OS X 10.6 SDK defines check() macro that interferes with our check()
// method
#ifdef check
#undef check
#endif

namespace blink {

class FontFaceCache;

using FontFaceSetIterable = SetlikeIterable<Member<FontFace>>;

class CORE_EXPORT FontFaceSet : public EventTargetWithInlineData,
                                public PausableObject,
                                public FontFaceSetIterable,
                                public FontFace::LoadFontCallback {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FontFaceSet(ExecutionContext& context)
      : PausableObject(&context),
        is_loading_(false),
        should_fire_loading_event_(false),
        ready_(new ReadyProperty(GetExecutionContext(),
                                 this,
                                 ReadyProperty::kReady)),
        async_runner_(AsyncMethodRunner<FontFaceSet>::Create(
            this,
            &FontFaceSet::HandlePendingEventsAndPromises,
            context.GetTaskRunner(TaskType::kInternalDefault))) {}
  ~FontFaceSet() override = default;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(loading);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingdone);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingerror);

  bool check(const String& font, const String& text, ExceptionState&);
  ScriptPromise load(ScriptState*, const String& font, const String& text);
  virtual ScriptPromise ready(ScriptState*) = 0;

  ExecutionContext* GetExecutionContext() const override {
    return PausableObject::GetExecutionContext();
  }

  const AtomicString& InterfaceName() const override {
    return EventTargetNames::FontFaceSet;
  }

  FontFaceSet* addForBinding(ScriptState*, FontFace*, ExceptionState&);
  void clearForBinding(ScriptState*, ExceptionState&);
  bool deleteForBinding(ScriptState*, FontFace*, ExceptionState&);
  bool hasForBinding(ScriptState*, FontFace*, ExceptionState&) const;

  void AddFontFacesToFontFaceCache(FontFaceCache*);

  // PausableObject
  void Pause() override;
  void Unpause() override;
  void ContextDestroyed(ExecutionContext*) override;

  wtf_size_t size() const;
  virtual AtomicString status() const = 0;

  void Trace(blink::Visitor*) override;

 protected:
  static const int kDefaultFontSize;
  static const char kDefaultFontFamily[];

  virtual bool ResolveFontStyle(const String&, Font&) = 0;
  virtual bool InActiveContext() const = 0;
  virtual FontSelector* GetFontSelector() const = 0;
  virtual const HeapLinkedHashSet<Member<FontFace>>& CSSConnectedFontFaceList()
      const = 0;
  bool IsCSSConnectedFontFace(FontFace* font_face) const {
    return CSSConnectedFontFaceList().Contains(font_face);
  }

  virtual void FireDoneEventIfPossible() = 0;

  void AddToLoadingFonts(FontFace*);
  void RemoveFromLoadingFonts(FontFace*);
  void HandlePendingEventsAndPromisesSoon();
  bool ShouldSignalReady() const;
  void FireDoneEvent();

  using ReadyProperty = ScriptPromiseProperty<Member<FontFaceSet>,
                                              Member<FontFaceSet>,
                                              Member<DOMException>>;

  bool is_loading_;
  bool should_fire_loading_event_;
  HeapLinkedHashSet<Member<FontFace>> non_css_connected_faces_;
  HeapHashSet<Member<FontFace>> loading_fonts_;
  FontFaceArray loaded_fonts_;
  FontFaceArray failed_fonts_;
  Member<ReadyProperty> ready_;

  Member<AsyncMethodRunner<FontFaceSet>> async_runner_;

  class IterationSource final : public FontFaceSetIterable::IterationSource {
   public:
    explicit IterationSource(const HeapVector<Member<FontFace>>& font_faces)
        : index_(0), font_faces_(font_faces) {}
    bool Next(ScriptState*,
              Member<FontFace>&,
              Member<FontFace>&,
              ExceptionState&) override;

    void Trace(blink::Visitor* visitor) override {
      visitor->Trace(font_faces_);
      FontFaceSetIterable::IterationSource::Trace(visitor);
    }

   private:
    wtf_size_t index_;
    HeapVector<Member<FontFace>> font_faces_;
  };

  class LoadFontPromiseResolver final
      : public GarbageCollectedFinalized<LoadFontPromiseResolver>,
        public FontFace::LoadFontCallback {
    USING_GARBAGE_COLLECTED_MIXIN(LoadFontPromiseResolver);

   public:
    static LoadFontPromiseResolver* Create(FontFaceArray faces,
                                           ScriptState* script_state) {
      return new LoadFontPromiseResolver(faces, script_state);
    }

    void LoadFonts();
    ScriptPromise Promise() { return resolver_->Promise(); }

    void NotifyLoaded(FontFace*) override;
    void NotifyError(FontFace*) override;

    void Trace(blink::Visitor*) override;

   private:
    LoadFontPromiseResolver(FontFaceArray faces, ScriptState* script_state)
        : num_loading_(faces.size()),
          error_occured_(false),
          resolver_(ScriptPromiseResolver::Create(script_state)) {
      font_faces_.swap(faces);
    }

    HeapVector<Member<FontFace>> font_faces_;
    int num_loading_;
    bool error_occured_;
    Member<ScriptPromiseResolver> resolver_;
  };

 private:
  FontFaceSetIterable::IterationSource* StartIteration(
      ScriptState*,
      ExceptionState&) override;

  void HandlePendingEventsAndPromises();
  void FireLoadingEvent();
  DISALLOW_COPY_AND_ASSIGN(FontFaceSet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SET_H_
