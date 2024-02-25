// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "content/public/test/render_view_test.h"
#include "gin/handle.h"
#include "gin/per_isolate_data.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8-context.h"

namespace content {

namespace {

class TestGinObject : public gin::Wrappable<TestGinObject> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<TestGinObject> Create(v8::Isolate* isolate, bool* alive) {
    return gin::CreateHandle(isolate, new TestGinObject(alive));
  }

  TestGinObject(const TestGinObject&) = delete;
  TestGinObject& operator=(const TestGinObject&) = delete;

 private:
  TestGinObject(bool* alive) : alive_(alive) { *alive_ = true; }
  ~TestGinObject() override { *alive_ = false; }

  raw_ptr<bool> alive_;
};

gin::WrapperInfo TestGinObject::kWrapperInfo = { gin::kEmbedderNativeGin };

class GinBrowserTest : public RenderViewTest {
 public:
  GinBrowserTest() {}

  GinBrowserTest(const GinBrowserTest&) = delete;
  GinBrowserTest& operator=(const GinBrowserTest&) = delete;

  ~GinBrowserTest() override {}

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        blink::switches::kJavaScriptFlags, "--expose_gc");

    RenderViewTest::SetUp();
  }
};

// Test that garbage collection doesn't crash if a gin-wrapped object is
// present.
TEST_F(GinBrowserTest, GinAndGarbageCollection) {
  LoadHTML("<!doctype html>");

  bool alive = false;

  {
    v8::Isolate* isolate = Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(GetMainFrame()->MainWorldScriptContext());

    // We create the object inside a scope so it's not kept alive by a handle
    // on the stack.
    TestGinObject::Create(isolate, &alive);
  }

  CHECK(alive);

  // Should not crash.
  Isolate()->LowMemoryNotification();

  CHECK(!alive);
}

} // namespace

}  // namespace content
