// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
#define CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/common/main_function_params.h"
#include "content/public/test/mock_policy_container_host.h"
#include "content/public/test/mock_render_thread.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "v8/include/v8-forward.h"

#if BUILDFLAG(IS_MAC)
#include <optional>

#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/stack_allocated.h"
#endif

namespace blink {
class PageState;
namespace scheduler {
class WebThreadScheduler;
}
struct VisualProperties;
class WebFrameWidget;
class WebGestureEvent;
class WebMouseEvent;
}

namespace gfx {
class Rect;
class Size;
}

namespace content {
class AgentSchedulingGroup;
class ContentBrowserClient;
class ContentClient;
class ContentRendererClient;
class FakeRenderWidgetHost;
class RendererMainPlatformDelegate;
class RendererBlinkPlatformImpl;
class RendererBlinkPlatformImplTestOverrideImpl;
class RenderFrame;
class RenderProcess;
class RenderView;

class RenderViewTest : public testing::Test {
 public:
  // A special BlinkPlatformImpl class with overrides that are useful for
  // RenderViewTest.
  class RendererBlinkPlatformImplTestOverride {
   public:
    RendererBlinkPlatformImplTestOverride();
    ~RendererBlinkPlatformImplTestOverride();
    RendererBlinkPlatformImpl* Get() const;
    void Initialize();
    void Shutdown();

    blink::scheduler::WebThreadScheduler* GetMainThreadScheduler() {
      return main_thread_scheduler_.get();
    }

   private:
    std::unique_ptr<blink::scheduler::WebThreadScheduler>
        main_thread_scheduler_;
    std::unique_ptr<RendererBlinkPlatformImplTestOverrideImpl>
        blink_platform_impl_;
  };

  // If |hook_render_frame_creation| is true then the RenderViewTest will hook
  // the RenderFrame creation so a TestRenderFrame is always created. If it is
  // false the subclass is responsible for hooking the create function.
  explicit RenderViewTest(bool hook_render_frame_creation = true);
  ~RenderViewTest() override;

 protected:
  // Returns a pointer to the main frame.
  blink::WebLocalFrame* GetMainFrame();
  RenderFrame* GetMainRenderFrame();
  v8::Isolate* Isolate();

  // Executes the given JavaScript in the context of the main frame.
  void ExecuteJavaScriptForTests(std::string_view js);

  // Executes the given JavaScript and sets the int value it evaluates to in
  // |result|.
  // Returns true if the JavaScript was evaluated correctly to an int value,
  // false otherwise.
  bool ExecuteJavaScriptAndReturnIntValue(const std::u16string& script,
                                          int* result);

  // Executes the given JavaScript and sets the number value it evaluates to in
  // |result|.
  // Returns true if the JavaScript was evaluated correctly to an number value,
  // false otherwise.
  bool ExecuteJavaScriptAndReturnNumberValue(const std::u16string& script,
                                             double* result);

  // Loads |html| into the main frame as a data: URL and blocks until the
  // navigation is committed.
  void LoadHTML(std::string_view html);

  // Pretends to load |url| into the main frame, but substitutes |html| for the
  // response body (and does not include any response headers). This can be used
  // instead of LoadHTML for tests that cannot use a data: url (for example if
  // document.location needs to be set to something specific.)
  void LoadHTMLWithUrlOverride(std::string_view html, std::string_view url);

  // Returns the current PageState.
  // In OOPIF enabled modes, this returns a PageState object for the main frame.
  blink::PageState GetCurrentPageState();

  // Navigates the main frame back or forward in session history and commits.
  // The caller must capture a PageState for the target page.
  void GoBack(const GURL& url, const blink::PageState& state);
  void GoForward(const GURL& url, const blink::PageState& state);

  // Sends one native key event over IPC.
  void SendNativeKeyEvent(const input::NativeWebKeyboardEvent& key_event);

  // Send a raw keyboard event to the renderer.
  void SendWebKeyboardEvent(const blink::WebKeyboardEvent& key_event);

  // Send a raw mouse event to the renderer.
  void SendWebMouseEvent(const blink::WebMouseEvent& mouse_event);

  // Send a raw gesture event to the renderer.
  void SendWebGestureEvent(const blink::WebGestureEvent& gesture_event);

  // Returns the bounds (coordinates and size) of the element with id
  // |element_id|.  Returns an empty rect if such an element was not found.
  gfx::Rect GetElementBounds(const std::string& element_id);

  // Sends a left mouse click in the middle of the element with id |element_id|.
  // Returns true if the event was sent, false otherwise (typically because
  // the element was not found).
  bool SimulateElementClick(const std::string& element_id);

  // Sends a left mouse click at the |point|.
  void SimulatePointClick(const gfx::Point& point);

  // Sends a right mouse click in the middle of the element with id
  // |element_id|. Returns true if the event was sent, false otherwise
  // (typically because the element was not found).
  bool SimulateElementRightClick(const std::string& element_id);

  // Sends a right mouse click at the |point|.
  void SimulatePointRightClick(const gfx::Point& point);

  // Sends a tap at the |rect|.
  void SimulateRectTap(const gfx::Rect& rect);

  // Simulates |element| being focused.
  void SetFocused(const blink::WebElement& element);

  // Simulates a null element being focused in |document|.
  void ChangeFocusToNull(const blink::WebDocument& document);

  // Simulates a navigation with a type of reload to the given url.
  void Reload(const GURL& url);

  // Resize the view.
  void Resize(gfx::Size new_size, bool is_fullscreen);

  // Simulates typing the |ascii_character| into this render view. Also accepts
  // ui::VKEY_BACK for backspace. Will flush the message loop if
  // |flush_message_loop| is true.
  void SimulateUserTypingASCIICharacter(char ascii_character,
                                        bool flush_message_loop);

  // Simulates user focusing |input|, erasing all text, and typing the
  // |new_value| instead. Will process input events for autofill. This is a user
  // gesture.
  void SimulateUserInputChangeForElement(blink::WebInputElement input,
                                         std::string_view new_value);

  // Same as SimulateUserInputChangeForElement, but takes the element's HTML id
  // attribute instead of the blink element.
  void SimulateUserInputChangeForElementById(std::string_view id,
                                             std::string_view new_value);

  // These are all methods from RenderViewImpl that we expose to testing code.
  void OnSameDocumentNavigation(blink::WebLocalFrame* frame,
                                bool is_new_navigation);

  blink::WebFrameWidget* GetWebFrameWidget();

  // Allows a subclass to override the various content client implementations.
  virtual ContentClient* CreateContentClient();
  virtual ContentBrowserClient* CreateContentBrowserClient();
  virtual ContentRendererClient* CreateContentRendererClient();
  virtual std::unique_ptr<FakeRenderWidgetHost> CreateRenderWidgetHost();

  // Allows a subclass to customize the initial size of the RenderView.
  virtual blink::VisualProperties InitialVisualProperties();

  // testing::Test
  void SetUp() override;

  void TearDown() override;

  // Install a fake URL loader factory for the RenderFrameImpl.
  void CreateFakeURLLoaderFactory();

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<RenderProcess> process_;
  // `web_view` is owned by the associated `RenderView` (which we do not store).
  // All allocated `RenderView`s will be destroyed in the `TearDown` method.
  mojo::AssociatedRemote<blink::mojom::PageBroadcast> page_broadcast_;
  raw_ptr<blink::WebView> web_view_ = nullptr;
  RendererBlinkPlatformImplTestOverride blink_platform_impl_;

  // These must outlive `content_client_`.
  std::unique_ptr<ContentBrowserClient> content_browser_client_;
  std::unique_ptr<ContentRendererClient> content_renderer_client_;

  std::unique_ptr<ContentClient> content_client_;
  std::unique_ptr<MockRenderThread> render_thread_;
  std::unique_ptr<AgentSchedulingGroup> agent_scheduling_group_;
  std::unique_ptr<FakeRenderWidgetHost> render_widget_host_;

  // The PolicyContainerHost for the main RenderFrameHost.
  std::unique_ptr<MockPolicyContainerHost> policy_container_host_;

  // Used to setup the process so renderers can run.
  std::unique_ptr<RendererMainPlatformDelegate> platform_;
  std::unique_ptr<MainFunctionParams> params_;
  std::unique_ptr<base::CommandLine> command_line_;

  // For Mojo.
  std::unique_ptr<base::TestIOThread> test_io_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
  mojo::BinderMap binders_;

#if BUILDFLAG(IS_MAC)
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  std::optional<base::apple::ScopedNSAutoreleasePool> autorelease_pool_;
#endif

 private:
  void GoToOffset(int offset, const GURL& url, const blink::PageState& state);
  void SendInputEvent(const blink::WebInputEvent& input_event);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
