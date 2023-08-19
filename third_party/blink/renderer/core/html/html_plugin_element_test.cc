// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_plugin_element.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/fake_web_plugin.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

class TestPluginLocalFrameClient : public EmptyLocalFrameClient {
 public:
  TestPluginLocalFrameClient() = default;

  int plugin_created_count() const { return plugin_created_count_; }

 private:
  WebPluginContainerImpl* CreatePlugin(HTMLPlugInElement& element,
                                       const KURL& url,
                                       const Vector<String>& param_names,
                                       const Vector<String>& param_values,
                                       const String& mime_type,
                                       bool load_manually) override {
    ++plugin_created_count_;

    // Based on LocalFrameClientImpl::CreatePlugin
    WebPluginParams params;
    params.url = url;
    params.mime_type = mime_type;
    params.attribute_names = param_names;
    params.attribute_values = param_values;
    params.load_manually = load_manually;

    WebPlugin* web_plugin = new FakeWebPlugin(params);
    if (!web_plugin)
      return nullptr;

    // The container takes ownership of the WebPlugin.
    auto* container =
        MakeGarbageCollected<WebPluginContainerImpl>(element, web_plugin);

    if (!web_plugin->Initialize(container))
      return nullptr;

    if (!element.GetLayoutObject())
      return nullptr;

    return container;
  }

  int plugin_created_count_ = 0;
};

}  // namespace

class HTMLPlugInElementTest : public PageTestBase,
                              public testing::WithParamInterface<const char*> {
 protected:
  void SetUp() final {
    frame_client_ = MakeGarbageCollected<TestPluginLocalFrameClient>();
    PageTestBase::SetupPageWithClients(nullptr, frame_client_, nullptr);
    GetFrame().GetSettings()->SetPluginsEnabled(true);
  }

  void TearDown() final {
    PageTestBase::TearDown();
    frame_client_ = nullptr;
  }

  LocalFrameView& GetFrameView() const {
    return GetDummyPageHolder().GetFrameView();
  }

  int plugin_created_count() const {
    return frame_client_->plugin_created_count();
  }

 private:
  Persistent<TestPluginLocalFrameClient> frame_client_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         HTMLPlugInElementTest,
                         testing::Values("embed", "object"));

TEST_P(HTMLPlugInElementTest, RemovePlugin) {
  constexpr char kDivWithPlugin[] = R"HTML(
    <div>
      <%s id='test_plugin'
          type='application/x-test-plugin'
          src='test_plugin'>
      </%s>
    </div>
  )HTML";

  const char* container_type = GetParam();
  GetDocument().body()->setInnerHTML(
      String::Format(kDivWithPlugin, container_type, container_type));

  auto* plugin = To<HTMLPlugInElement>(
      GetDocument().getElementById(AtomicString("test_plugin")));
  ASSERT_TRUE(plugin);
  EXPECT_EQ(container_type, plugin->tagName().LowerASCII());

  UpdateAllLifecyclePhasesForTest();
  plugin->UpdatePlugin();

  EXPECT_EQ(1, plugin_created_count());

  auto* owned_plugin = plugin->OwnedPlugin();
  ASSERT_TRUE(owned_plugin);

  EXPECT_EQ(1u, GetFrameView().Plugins().size());
  ASSERT_TRUE(GetFrameView().Plugins().Contains(owned_plugin));

  plugin->parentNode()->removeChild(plugin);
  EXPECT_FALSE(GetDocument().HasElementWithId(AtomicString("test_plugin")));

  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, GetFrameView().Plugins().size());
  EXPECT_FALSE(GetFrameView().Plugins().Contains(owned_plugin));
}

}  // namespace blink
