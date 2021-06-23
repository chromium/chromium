// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "gin/handle.h"
#include "gin/per_isolate_data.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

namespace {

class TestGinObject : public gin::Wrappable<TestGinObject> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<TestGinObject> Create(v8::Isolate* isolate, bool* alive) {
    return gin::CreateHandle(isolate, new TestGinObject(alive));
  }

 private:
  TestGinObject(bool* alive) : alive_(alive) { *alive_ = true; }
  ~TestGinObject() override { *alive_ = false; }

  bool* alive_;

  DISALLOW_COPY_AND_ASSIGN(TestGinObject);
};

gin::WrapperInfo TestGinObject::kWrapperInfo = { gin::kEmbedderNativeGin };

class GinBrowserTest : public RenderViewTest {
 public:
  GinBrowserTest() {}
  ~GinBrowserTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kJavaScriptFlags, "--expose_gc");

    RenderViewTest::SetUp();
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(GinBrowserTest);
};

// Test that garbage collection doesn't crash if a gin-wrapped object is
// present.
TEST_F(GinBrowserTest, GinAndGarbageCollection) {
  LoadHTML("<!doctype html>");

  bool alive = false;

  {
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(GetMainFrame()->MainWorldScriptContext());

    // We create the object inside a scope so it's not kept alive by a handle
    // on the stack.
    TestGinObject::Create(isolate, &alive);
  }

  CHECK(alive);

  // Should not crash.
  blink::MainThreadIsolate()->LowMemoryNotification();

  CHECK(!alive);
}

} // namespace

}  // namespace content
