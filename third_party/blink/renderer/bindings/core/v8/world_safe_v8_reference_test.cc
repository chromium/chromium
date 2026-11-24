// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"

#include <string_view>
#include <utility>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "v8/include/v8.h"

namespace blink {

class DummyPageHolder;
class KURL;

namespace {

constexpr int32_t kIsolatedWorldId1 = 1;

v8::Local<v8::Value> EvalInScriptState(ScriptState* script_state,
                                       std::string_view source) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Context::Scope context_scope(context);
  V8DoNotRunMicrotasksScope microtasks_scope(script_state);
  v8::Local<v8::String> v8_source =
      v8::String::NewFromUtf8(isolate, source.data(),
                              v8::NewStringType::kNormal,
                              static_cast<int>(source.size()))
          .ToLocalChecked();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, v8_source).ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

struct ReferenceHolder final : public GarbageCollected<ReferenceHolder> {
  WorldSafeV8Reference<v8::Value> reference;

  void Trace(Visitor* visitor) const { visitor->Trace(reference); }
};

class IsolateOnlyV8TestingScope {
  STACK_ALLOCATED();

 public:
  IsolateOnlyV8TestingScope(const KURL& url = KURL())
      : holder_(DummyPageHolder::CreateAndCommitNavigation(url)),
        handle_scope_(GetIsolate()) {}

  v8::Isolate* GetIsolate() const {
    return ToScriptStateForMainWorld(holder_->GetDocument().GetFrame())
        ->GetIsolate();
  }

 private:
  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
};

// http://crbug.com/1007504, http://crbug.com/1008425
TEST(WorldSafeV8ReferenceTest, CreatedWhenNotInContext) {
  test::TaskEnvironment task_environment;
  WorldSafeV8Reference<v8::Value> v8_reference;
  v8::Local<v8::Value> value;
  {
    IsolateOnlyV8TestingScope scope1;
    v8::Isolate* isolate = scope1.GetIsolate();
    CHECK(isolate);
    CHECK(!isolate->InContext());

    value = v8::Null(isolate);
    v8_reference = WorldSafeV8Reference<v8::Value>(isolate, value);
    EXPECT_FALSE(v8_reference.IsEmpty());
  }
  V8TestingScope scope2;
  ScriptState* script_state = scope2.GetScriptState();
  EXPECT_EQ(v8_reference.Get(script_state), value);
}

TEST(WorldSafeV8ReferenceTest, OutOfContextPrimitiveAndObjectGetAcrossWorld) {
  test::TaskEnvironment task_environment;
  auto holder =
      DummyPageHolder::CreateAndCommitNavigation(KURL("https://example.com/"));
  ScriptState* main_state =
      ToScriptStateForMainWorld(holder->GetDocument().GetFrame());
  v8::Isolate* isolate = main_state->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  ScriptState* isolated_state = ToScriptState(
      holder->GetDocument().domWindow(),
      *DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1));

  v8::Local<v8::Value> main_object = EvalInScriptState(main_state, R"JS(
    (() => {
      globalThis.capturedArgs = null;
      const obj = {};
      Object.defineProperty(obj, 'x', {
        enumerable: true,
        get: new Proxy(() => {}, {
          apply(_t, _r, args) { globalThis.capturedArgs = args; return 17; }
        }),
      });
      return obj;
    })();
  )JS");

  ASSERT_FALSE(isolate->InContext());
  WorldSafeV8Reference<v8::Value> int_ref(isolate,
                                          v8::Integer::New(isolate, 42));
  WorldSafeV8Reference<v8::Value> obj_ref(isolate, main_object);

  {
    ScriptState::Scope isolated_scope(isolated_state);
    v8::Local<v8::Value> int_val = int_ref.GetAcrossWorld(isolated_state);
    ASSERT_TRUE(int_val->IsInt32());
    EXPECT_EQ(42, int_val.As<v8::Int32>()->Value());
    v8::Local<v8::Value> cloned = obj_ref.GetAcrossWorld(isolated_state);
    ASSERT_TRUE(cloned->IsObject());
    EXPECT_EQ(cloned.As<v8::Object>()->GetCreationContextChecked(isolate),
              isolated_state->GetContext());
  }

  v8::Local<v8::Value> captured_args =
      EvalInScriptState(main_state, "globalThis.capturedArgs");
  ASSERT_TRUE(captured_args->IsArray());
  EXPECT_EQ(captured_args.As<v8::Object>()->GetCreationContextChecked(isolate),
            main_state->GetContext());
}

TEST(WorldSafeV8ReferenceTest, CrossWorldSerializationRunsInSourceRealm) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.com/"));
  v8::Isolate* isolate = scope.GetIsolate();
  ScriptState* main_state = scope.GetScriptState();
  ScriptState* isolated_state = ToScriptState(
      &scope.GetWindow(),
      *DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1));

  WorldSafeV8Reference<v8::Value> ref;
  {
    ScriptState::Scope source_scope(main_state);
    v8::Local<v8::Value> payload = EvalInScriptState(main_state, R"JS(
      (() => {
        globalThis.capturedArgs = null;
        globalThis.capturedSites = null;
        Error.prepareStackTrace = (_err, sites) => {
          globalThis.capturedSites = sites;
          return 'main_trace';
        };
        const obj = { err: new Error('boom') };
        Object.defineProperty(obj, 'computed', {
          enumerable: true,
          get: new Proxy(() => {}, {
            apply(_t, _r, args) { globalThis.capturedArgs = args; return 42; }
          }),
        });
        return obj;
      })();
    )JS");
    ref.Set(isolate, payload);
  }

  {
    ScriptState::Scope target_scope(isolated_state);
    v8::Local<v8::Value> cloned = ref.GetAcrossWorld(isolated_state);
    ASSERT_TRUE(cloned->IsObject());
    EXPECT_EQ(cloned.As<v8::Object>()->GetCreationContextChecked(isolate),
              isolated_state->GetContext());
  }

  v8::Local<v8::Value> args =
      EvalInScriptState(main_state, "globalThis.capturedArgs");
  ASSERT_TRUE(args->IsArray());
  EXPECT_EQ(args.As<v8::Object>()->GetCreationContextChecked(isolate),
            main_state->GetContext());

  v8::Local<v8::Value> sites =
      EvalInScriptState(main_state, "globalThis.capturedSites");
  ASSERT_TRUE(sites->IsArray());
  EXPECT_EQ(sites.As<v8::Object>()->GetCreationContextChecked(isolate),
            main_state->GetContext());
}

TEST(WorldSafeV8ReferenceTest, CrossWorldBoundNativeGetterUsesSourceIncumbent) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.com/"));
  v8::Isolate* isolate = scope.GetIsolate();
  ScriptState* main_state = scope.GetScriptState();
  ScriptState* isolated_state = ToScriptState(
      &scope.GetWindow(),
      *DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1));

  struct State {
    STACK_ALLOCATED();

   public:
    ScriptState* expected_incumbent;
    ScriptState* target_state;
    WorldSafeV8Reference<v8::Value> ref;
    bool getter_called = false;
    bool incumbent_matched = false;
  } state{main_state, isolated_state};

  {
    ScriptState::Scope main_scope(main_state);
    v8::Local<v8::Context> main_ctx = main_state->GetContext();
    v8::Local<v8::Function> getter =
        v8::FunctionTemplate::New(
            isolate,
            [](const v8::FunctionCallbackInfo<v8::Value>& info) {
              auto* s =
                  static_cast<State*>(info.Data().As<v8::External>()->Value(
                      v8::kExternalPointerTypeTagDefault));
              s->getter_called = true;
              s->incumbent_matched = info.GetIsolate()->GetIncumbentContext() ==
                                     s->expected_incumbent->GetContext();
              info.GetReturnValue().Set(1);
            },
            v8::External::New(isolate, &state,
                              v8::kExternalPointerTypeTagDefault))
            ->GetFunction(main_ctx)
            .ToLocalChecked();
    v8::Local<v8::Object> obj = v8::Object::New(isolate);
    obj->SetAccessorProperty(V8String(isolate, "x"), getter, {}, v8::None);
    state.ref.Set(isolate, obj);
  }

  {
    ScriptState::Scope isolated_scope(isolated_state);
    v8::Local<v8::Context> isolated_ctx = isolated_state->GetContext();
    v8::Local<v8::Function> read_ref =
        v8::FunctionTemplate::New(
            isolate,
            [](const v8::FunctionCallbackInfo<v8::Value>& info) {
              auto* s =
                  static_cast<State*>(info.Data().As<v8::External>()->Value(
                      v8::kExternalPointerTypeTagDefault));
              s->ref.GetAcrossWorld(s->target_state);
            },
            v8::External::New(isolate, &state,
                              v8::kExternalPointerTypeTagDefault))
            ->GetFunction(isolated_ctx)
            .ToLocalChecked();
    ASSERT_TRUE(isolated_ctx->Global()
                    ->CreateDataProperty(isolated_ctx,
                                         V8String(isolate, "readRef"), read_ref)
                    .FromMaybe(false));
  }

  EvalInScriptState(isolated_state, "readRef()");
  EXPECT_TRUE(state.getter_called);
  EXPECT_TRUE(state.incumbent_matched);
}

TEST(WorldSafeV8ReferenceTest, CrossWorldSharedArrayBufferUnpacksCleanly) {
  ScopedSharedArrayBufferForTest enable_sab(true);
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.com/"));
  v8::Isolate* isolate = scope.GetIsolate();
  ScriptState* main_state = scope.GetScriptState();
  ScriptState* isolated_state = ToScriptState(
      &scope.GetWindow(),
      *DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1));

  WorldSafeV8Reference<v8::Value> v8_reference(
      isolate,
      EvalInScriptState(main_state, "({ buffer: new SharedArrayBuffer(16) })"));
  ScriptState::Scope isolated_scope(isolated_state);
  v8::Local<v8::Value> cloned = v8_reference.GetAcrossWorld(isolated_state);
  ASSERT_TRUE(cloned->IsObject());
  EXPECT_EQ(cloned.As<v8::Object>()->GetCreationContextChecked(isolate),
            isolated_state->GetContext());
}

class WorldSafeV8ReferenceSimTest : public SimTest {};

TEST_F(WorldSafeV8ReferenceSimTest,
       DetachedContextsCloneInSourceRealmAndDoNotLeak) {
  ScopedSharedArrayBufferForTest enable_sab(true);
  SimRequest main_resource("https://example.com/main.html", "text/html");
  SimRequest detach_src("https://example.com/detach_src.html", "text/html");
  SimRequest detach_dst("https://example.com/detach_dst.html", "text/html");
  LoadURL("https://example.com/main.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <iframe id="detach_src" src="https://example.com/detach_src.html"></iframe>
    <iframe id="detach_dst" src="https://example.com/detach_dst.html"></iframe>
  )HTML");
  detach_src.Complete("<!DOCTYPE html>");
  detach_dst.Complete("<!DOCTYPE html>");
  test::RunPendingTasks();

  v8::Isolate* isolate = Window().GetIsolate();
  Persistent<DOMWrapperWorld> isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1);
  ASSERT_TRUE(isolated_world);

  Persistent<ReferenceHolder> detached_obj_holder =
      MakeGarbageCollected<ReferenceHolder>();
  Persistent<ReferenceHolder> detached_blob_holder =
      MakeGarbageCollected<ReferenceHolder>();
  Persistent<ReferenceHolder> detached_bitmap_holder =
      MakeGarbageCollected<ReferenceHolder>();
  Persistent<ReferenceHolder> detached_wasm_holder =
      MakeGarbageCollected<ReferenceHolder>();
  Persistent<ReferenceHolder> detached_sab_holder =
      MakeGarbageCollected<ReferenceHolder>();
  Persistent<ReferenceHolder> detached_prim_holder =
      MakeGarbageCollected<ReferenceHolder>();
  WeakPersistent<LocalDOMWindow> weak_detach_src_window;

  {
    v8::HandleScope handle_scope(isolate);
    Frame* detach_src_frame = GetDocument().GetFrame()->Tree().FirstChild();
    Frame* detach_dst_frame = detach_src_frame->Tree().NextSibling();
    ScriptState* main_state =
        ToScriptStateForMainWorld(GetDocument().GetFrame());
    ScriptState* detach_src_state =
        ToScriptStateForMainWorld(To<LocalFrame>(detach_src_frame));
    ScriptState* detach_dst_isolated_state = ToScriptState(
        To<LocalFrame>(detach_dst_frame)->DomWindow(), *isolated_world);
    weak_detach_src_window = To<LocalFrame>(detach_src_frame)->DomWindow();

    // Getter that synchronously detaches `target_script_state` fails closed.
    WorldSafeV8Reference<v8::Value> detach_target_ref;
    {
      ScriptState::Scope scope(main_state);
      detach_target_ref.Set(isolate, EvalInScriptState(main_state, R"JS(
        ({ get x() { document.getElementById('detach_dst').remove(); return 1; } })
      )JS"));
    }
    {
      ScriptState::Scope caller_scope(main_state);
      EXPECT_TRUE(detach_target_ref.GetAcrossWorld(detach_dst_isolated_state)
                      ->IsNull());
    }

    // Set up detached source frame references for post-GC verification below.
    EvalInScriptState(main_state,
                      "globalThis.getterUsedDetachedRealm = false;");
    {
      ScriptState::Scope scope(detach_src_state);
      detached_obj_holder->reference.Set(
          isolate, EvalInScriptState(detach_src_state, R"JS(
            (() => {
              const parentWin = parent;
              const detachedArrayCtor = Array;
              const obj = {};
              Object.defineProperty(obj, 'a', {
                enumerable: true,
                get: new Proxy(() => {}, {
                  apply(_t, _r, args) {
                    parentWin.getterUsedDetachedRealm =
                        (Object.getPrototypeOf(args) ===
                         detachedArrayCtor.prototype);
                    return 42;
                  }
                }),
              });
              return obj;
            })()
          )JS"));
      detached_blob_holder->reference.Set(
          isolate, EvalInScriptState(detach_src_state, "new Blob(['hello'])"));
      detached_bitmap_holder->reference.Set(
          isolate, EvalInScriptState(detach_src_state, R"JS(
            (() => {
              const c = new OffscreenCanvas(1, 1);
              c.getContext('2d');
              return c.transferToImageBitmap();
            })()
          )JS"));
      detached_wasm_holder->reference.Set(
          isolate, EvalInScriptState(detach_src_state,
                                     "new WebAssembly.Module(new "
                                     "Uint8Array([0,97,115,109,1,0,0,0]))"));
      detached_sab_holder->reference.Set(
          isolate,
          EvalInScriptState(detach_src_state, "new SharedArrayBuffer(16)"));
      detached_prim_holder->reference.Set(isolate,
                                          v8::Integer::New(isolate, 7));
    }
    EvalInScriptState(main_state,
                      "document.getElementById('detach_src').remove();");
  }

  test::RunPendingTasks();
  isolate->LowMemoryNotification();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  {
    v8::HandleScope handle_scope(isolate);
    ScriptState* main_isolated_state =
        ToScriptState(&Window(), *isolated_world);
    ScriptState::Scope target_scope(main_isolated_state);

    // JS objects serialize in the detached source realm without leaking
    // target-world intrinsics.
    v8::Local<v8::Value> cloned_obj =
        detached_obj_holder->reference.GetAcrossWorld(main_isolated_state);
    ASSERT_TRUE(cloned_obj->IsObject());
    EXPECT_EQ(cloned_obj.As<v8::Object>()->GetCreationContextChecked(isolate),
              main_isolated_state->GetContext());
    v8::Local<v8::Value> prop_a = cloned_obj.As<v8::Object>()
                                      ->Get(main_isolated_state->GetContext(),
                                            V8AtomicString(isolate, "a"))
                                      .ToLocalChecked();
    ASSERT_TRUE(prop_a->IsInt32());
    EXPECT_EQ(42, prop_a.As<v8::Int32>()->Value());
    EXPECT_TRUE(
        EvalInScriptState(ToScriptStateForMainWorld(GetDocument().GetFrame()),
                          "globalThis.getterUsedDetachedRealm")
            ->BooleanValue(isolate));

    // Host/DOM and Wasm module objects that do not depend on the source
    // ExecutionContext still clone cleanly, while SharedArrayBuffer fails
    // closed when the source ExecutionContext is detached/destroyed.
    EXPECT_TRUE(
        detached_blob_holder->reference.GetAcrossWorld(main_isolated_state)
            ->IsObject());
    EXPECT_TRUE(
        detached_bitmap_holder->reference.GetAcrossWorld(main_isolated_state)
            ->IsObject());
    EXPECT_TRUE(
        detached_wasm_holder->reference.GetAcrossWorld(main_isolated_state)
            ->IsObject());
    EXPECT_TRUE(
        detached_sab_holder->reference.GetAcrossWorld(main_isolated_state)
            ->IsNull());

    v8::Local<v8::Value> prim_val =
        detached_prim_holder->reference.GetAcrossWorld(main_isolated_state);
    ASSERT_TRUE(prim_val->IsInt32());
    EXPECT_EQ(7, prim_val.As<v8::Int32>()->Value());
  }

  detached_obj_holder->reference.Reset();
  detached_blob_holder->reference.Reset();
  detached_bitmap_holder->reference.Reset();
  detached_wasm_holder->reference.Reset();
  detached_sab_holder->reference.Reset();
  isolate->LowMemoryNotification();
  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);
  EXPECT_FALSE(weak_detach_src_window);
}

TEST_F(WorldSafeV8ReferenceSimTest,
       DetachedIframeCustomEventDetailClonesAcrossWorlds) {
  SimRequest main_resource("https://example.com/main.html", "text/html");
  LoadURL("https://example.com/main.html");
  main_resource.Complete("<!DOCTYPE html><body></body>");
  test::RunPendingTasks();

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  Persistent<DOMWrapperWorld> isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1);
  ASSERT_TRUE(isolated_world);

  ScriptState* main_state = ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState* isolated_state = ToScriptState(&Window(), *isolated_world);

  EvalInScriptState(isolated_state, R"JS(
    globalThis.received = [];
    window.addEventListener('probe', e => {
      const d = e.detail;
      if (d instanceof Blob) {
        globalThis.received.push({ blobSize: d.size });
      } else {
        globalThis.received.push({
          isNull: d === null,
          inIsolatedRealm: d !== null && typeof d === 'object' &&
                           Object.getPrototypeOf(d) === Object.prototype,
          json: JSON.stringify(d),
        });
      }
    });
  )JS");

  EvalInScriptState(main_state, R"JS(
    const f = document.createElement('iframe');
    document.body.appendChild(f);
    const W = f.contentWindow;
    const DetachedCE = W.CustomEvent;
    f.remove();

    // Plain object created in the live main page.
    dispatchEvent(new DetachedCE('probe', { detail: { a: 1 } }));
    // Null-prototype object created via the detached iframe's primordials.
    const nullProto = W.Object.assign(W.Object.create(null), { m: 'ack' });
    dispatchEvent(new DetachedCE('probe', { detail: nullProto }));
    // Primitive detail.
    dispatchEvent(new DetachedCE('probe', { detail: 'str' }));
    // Host/DOM object also clones across worlds.
    dispatchEvent(new DetachedCE('probe', { detail: new Blob(['x']) }));
  )JS");

  v8::Local<v8::Value> summary =
      EvalInScriptState(isolated_state, "JSON.stringify(globalThis.received)");
  ASSERT_TRUE(summary->IsString());
  EXPECT_EQ(
      R"([{"isNull":false,"inIsolatedRealm":true,"json":"{\"a\":1}"},)"
      R"({"isNull":false,"inIsolatedRealm":true,"json":"{\"m\":\"ack\"}"},)"
      R"({"isNull":false,"inIsolatedRealm":false,"json":"\"str\""},)"
      R"({"blobSize":1}])",
      ToCoreString(isolate, summary.As<v8::String>()));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(WorldSafeV8ReferenceSimTest,
       CrossFrameSharedArrayBufferRespectsTargetContextPolicy) {
  ScopedSharedArrayBufferForTest disable_sab(false);
  ScopedSharedArrayBufferUnrestrictedAccessAllowedForTest disable_unrestricted(
      false);

  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(
      "<!DOCTYPE html><html><body><iframe></iframe></body></html>");

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  LocalFrame* main_frame = GetDocument().GetFrame();
  auto* child_frame = DynamicTo<LocalFrame>(main_frame->Tree().FirstChild());
  ASSERT_TRUE(child_frame);

  main_frame->DomWindow()->GetOriginTrialContext()->AddFeature(
      mojom::blink::OriginTrialFeature::kUnrestrictedSharedArrayBuffer);
  ASSERT_TRUE(main_frame->DomWindow()->SharedArrayBufferTransferAllowed());
  ASSERT_FALSE(child_frame->DomWindow()->SharedArrayBufferTransferAllowed());

  ScriptState* main_script_state = ToScriptStateForMainWorld(main_frame);
  WorldSafeV8Reference<v8::Value> v8_reference;
  {
    ScriptState::Scope main_scope(main_script_state);
    v8_reference.Set(
        isolate, EvalInScriptState(main_script_state,
                                   "({ buffer: new SharedArrayBuffer(16) })"));
  }

  ScriptState* child_isolated_state = ToScriptState(
      child_frame,
      *DOMWrapperWorld::EnsureIsolatedWorld(isolate, kIsolatedWorldId1));
  ScriptState::Scope isolated_scope(child_isolated_state);
  EXPECT_TRUE(v8_reference.GetAcrossWorld(child_isolated_state)->IsNull());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

}  // namespace blink
