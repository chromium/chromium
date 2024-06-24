// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/text_input_test_utils.h"

#include <memory>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "components/input/render_widget_host_view_input_observer.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#endif

namespace ui {
class TextInputClient;
}

namespace content {

// This class is an observer of TextInputManager associated with the provided
// WebContents. An instance of this class is used in TextInputManagerTester to
// expose the required API for testing outside of content/.
class TextInputManagerTester::InternalObserver
    : public TextInputManager::Observer,
      public WebContentsObserver {
 public:
  InternalObserver(WebContents* web_contents, TextInputManagerTester* tester)
      : WebContentsObserver(web_contents),
        updated_view_(nullptr),
        text_input_state_changed_(false) {
    text_input_manager_ =
        static_cast<WebContentsImpl*>(web_contents)->GetTextInputManager();
    DCHECK(!!text_input_manager_);
    text_input_manager_->AddObserver(this);
  }

  InternalObserver(const InternalObserver&) = delete;
  InternalObserver& operator=(const InternalObserver&) = delete;

  ~InternalObserver() override {
    if (text_input_manager_)
      text_input_manager_->RemoveObserver(this);
  }

  void set_update_text_input_state_called_callback(
      base::RepeatingClosure callback) {
    update_text_input_state_callback_ = std::move(callback);
  }

  void set_on_selection_bounds_changed_callback(
      base::RepeatingClosure callback) {
    on_selection_bounds_changed_callback_ = std::move(callback);
  }

  void set_on_ime_composition_range_changed_callback(
      base::RepeatingClosure callback) {
    on_ime_composition_range_changed_callback_ = std::move(callback);
  }

  void set_on_text_selection_changed_callback(base::RepeatingClosure callback) {
    on_text_selection_changed_callback_ = std::move(callback);
  }

  const gfx::Range* last_composition_range() const {
    return last_composition_range_.get();
  }

  RenderWidgetHostView* GetUpdatedView() const { return updated_view_; }

  bool text_input_state_changed() const { return text_input_state_changed_; }

  TextInputManager* text_input_manager() const { return text_input_manager_; }

  // TextInputManager::Observer implementations.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_change_state) override {
    if (text_input_manager_ != text_input_manager)
      return;
    text_input_state_changed_ = did_change_state;
    updated_view_ = updated_view;
    if (!update_text_input_state_callback_.is_null())
      update_text_input_state_callback_.Run();
  }

  void OnSelectionBoundsChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override {
    updated_view_ = updated_view;
    if (!on_selection_bounds_changed_callback_.is_null())
      on_selection_bounds_changed_callback_.Run();
  }

  void OnImeCompositionRangeChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view,
      bool character_bounds_changed,
      const std::optional<std::vector<gfx::Rect>>& line_bounds) override {
    updated_view_ = updated_view;
    const gfx::Range* range =
        text_input_manager_->GetCompositionRangeForTesting();
    DCHECK(range);
    last_composition_range_ =
        std::make_unique<gfx::Range>(range->start(), range->end());
    if (!on_ime_composition_range_changed_callback_.is_null())
      on_ime_composition_range_changed_callback_.Run();
  }

  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override {
    updated_view_ = updated_view;
    if (!on_text_selection_changed_callback_.is_null())
      on_text_selection_changed_callback_.Run();
  }

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    DCHECK(text_input_manager_);
    text_input_manager_->RemoveObserver(this);
    text_input_manager_ = nullptr;
  }

 private:
  raw_ptr<TextInputManager> text_input_manager_;
  raw_ptr<RenderWidgetHostViewBase> updated_view_;
  bool text_input_state_changed_;
  std::unique_ptr<gfx::Range> last_composition_range_;
  base::RepeatingClosure update_text_input_state_callback_;
  base::RepeatingClosure on_selection_bounds_changed_callback_;
  base::RepeatingClosure on_ime_composition_range_changed_callback_;
  base::RepeatingClosure on_text_selection_changed_callback_;
};

// This class observes the lifetime of a RenderWidgetHostView. An instance of
// this class is used in TestRenderWidgetHostViewDestructionObserver to expose
// the required observer API for testing outside of content/.
class TestRenderWidgetHostViewDestructionObserver::InternalObserver
    : public input::RenderWidgetHostViewInputObserver {
 public:
  InternalObserver(RenderWidgetHostViewBase* view)
      : view_(view), destroyed_(false) {
    view->AddObserver(this);
  }

  InternalObserver(const InternalObserver&) = delete;
  InternalObserver& operator=(const InternalObserver&) = delete;

  ~InternalObserver() override {
    if (view_)
      view_->RemoveObserver(this);
  }

  void Wait() {
    if (destroyed_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner();
    message_loop_runner_->Run();
  }

 private:
  void OnRenderWidgetHostViewInputDestroyed(
      input::RenderWidgetHostViewInput* view) override {
    DCHECK_EQ(view_, view);
    destroyed_ = true;
    view->RemoveObserver(this);
    view_ = nullptr;
    if (message_loop_runner_)
      message_loop_runner_->Quit();
  }

  raw_ptr<RenderWidgetHostViewBase> view_;
  bool destroyed_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

#if defined(USE_AURA)
class InputMethodObserverAura : public TestInputMethodObserver,
                                public ui::InputMethodObserver {
 public:
  explicit InputMethodObserverAura(ui::InputMethod* input_method)
      : input_method_(input_method), text_input_client_(nullptr) {
    input_method_->AddObserver(this);
  }

  InputMethodObserverAura(const InputMethodObserverAura&) = delete;
  InputMethodObserverAura& operator=(const InputMethodObserverAura&) = delete;

  ~InputMethodObserverAura() override {
    if (input_method_)
      input_method_->RemoveObserver(this);
  }

  // TestInputMethodObserver implementations.
  ui::TextInputType GetTextInputTypeFromClient() override {
    if (text_input_client_)
      return text_input_client_->GetTextInputType();

    return ui::TEXT_INPUT_TYPE_NONE;
  }

  void SetOnVirtualKeyboardVisibilityChangedIfEnabledCallback(
      const base::RepeatingCallback<void(bool)>& callback) override {
    on_virtual_keyboard_visibility_changed_if_enabled_callback_ = callback;
  }

 private:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnTextInputStateChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

  void OnVirtualKeyboardVisibilityChangedIfEnabled(bool should_show) override {
    on_virtual_keyboard_visibility_changed_if_enabled_callback_.Run(
        should_show);
  }

  raw_ptr<ui::InputMethod> input_method_;
  raw_ptr<const ui::TextInputClient> text_input_client_;
  base::RepeatingCallback<void(bool)>
      on_virtual_keyboard_visibility_changed_if_enabled_callback_;
};
#endif

ui::TextInputType GetTextInputTypeFromWebContents(WebContents* web_contents) {
  const ui::mojom::TextInputState* state =
      static_cast<WebContentsImpl*>(web_contents)
          ->GetTextInputManager()
          ->GetTextInputState();
  return !!state ? state->type : ui::TEXT_INPUT_TYPE_NONE;
}

const ui::mojom::TextInputState* GetTextInputStateFromWebContents(
    WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetTextInputManager()
      ->GetTextInputState();
}

bool GetTextInputTypeForView(WebContents* web_contents,
                             RenderWidgetHostView* view,
                             ui::TextInputType* type) {
  TextInputManager* manager =
      static_cast<WebContentsImpl*>(web_contents)->GetTextInputManager();

  RenderWidgetHostViewBase* view_base =
      static_cast<RenderWidgetHostViewBase*>(view);
  if (!manager || !manager->IsRegistered(view_base))
    return false;

  *type = manager->GetTextInputTypeForViewForTesting(view_base);

  return true;
}

bool RequestCompositionInfoFromActiveWidget(WebContents* web_contents) {
  TextInputManager* manager =
      static_cast<WebContentsImpl*>(web_contents)->GetTextInputManager();
  if (!manager || !manager->GetActiveWidget())
    return false;

  manager->GetActiveWidget()->RequestCompositionUpdates(
      true /* immediate_request */, false /* monitor_updates */);
  return true;
}

bool DoesFrameHaveFocusedEditableElement(RenderFrameHost* frame) {
  return static_cast<RenderFrameHostImpl*>(frame)
      ->has_focused_editable_element();
}

void SendImeCommitTextToWidget(
    RenderWidgetHost* rwh,
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos) {
  RenderWidgetHostImpl::From(rwh)->ImeCommitText(
      text, ime_text_spans, replacement_range, relative_cursor_pos);
}

void SendImeSetCompositionTextToWidget(
    RenderWidgetHost* rwh,
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end) {
  RenderWidgetHostImpl::From(rwh)->ImeSetComposition(
      text, ime_text_spans, replacement_range, selection_start, selection_end);
}

void SendTextInputStateChangedToWidget(RenderWidgetHost* rwh,
                                       ui::mojom::TextInputStatePtr state) {
  RenderWidgetHostImpl::From(rwh)->TextInputStateChanged(std::move(state));
}

bool DestroyRenderWidgetHost(int32_t process_id,
                             int32_t local_root_routing_id) {
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(process_id, local_root_routing_id);
  if (!rfh)
    return false;

  while (!rfh->is_local_root())
    rfh = rfh->GetParent();

  DCHECK(rfh->GetPage().IsPrimary())
      << "Only implemented for frames in a primary page";
  FrameTreeNode* ftn = rfh->frame_tree_node();
  if (rfh->IsOutermostMainFrame()) {
    WebContents::FromRenderFrameHost(rfh)->Close();
  } else {
    ftn->frame_tree().RemoveFrame(ftn);
  }
  return true;
}

size_t GetRegisteredViewsCountFromTextInputManager(WebContents* web_contents) {
  std::unordered_set<RenderWidgetHostView*> views;
  TextInputManager* manager =
      static_cast<WebContentsImpl*>(web_contents)->GetTextInputManager();
  return !!manager ? manager->GetRegisteredViewsCountForTesting() : 0;
}

RenderWidgetHostView* GetActiveViewFromWebContents(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetTextInputManager()
      ->active_view_for_testing();
}

TextInputManagerTester::TextInputManagerTester(WebContents* web_contents)
    : observer_(new InternalObserver(web_contents, this)) {}

TextInputManagerTester::~TextInputManagerTester() {}

void TextInputManagerTester::SetUpdateTextInputStateCalledCallback(
    base::RepeatingClosure callback) {
  observer_->set_update_text_input_state_called_callback(std::move(callback));
}

void TextInputManagerTester::SetOnSelectionBoundsChangedCallback(
    base::RepeatingClosure callback) {
  observer_->set_on_selection_bounds_changed_callback(std::move(callback));
}

void TextInputManagerTester::SetOnImeCompositionRangeChangedCallback(
    base::RepeatingClosure callback) {
  observer_->set_on_ime_composition_range_changed_callback(std::move(callback));
}

void TextInputManagerTester::SetOnTextSelectionChangedCallback(
    base::RepeatingClosure callback) {
  observer_->set_on_text_selection_changed_callback(std::move(callback));
}

bool TextInputManagerTester::GetTextInputType(ui::TextInputType* type) {
  DCHECK(observer_->text_input_manager());
  const ui::mojom::TextInputState* state =
      observer_->text_input_manager()->GetTextInputState();
  if (!state)
    return false;
  *type = state->type;
  return true;
}

bool TextInputManagerTester::GetTextInputValue(std::string* value) {
  DCHECK(observer_->text_input_manager());
  const ui::mojom::TextInputState* state =
      observer_->text_input_manager()->GetTextInputState();
  if (!state)
    return false;
  if (state->value)
    *value = base::UTF16ToUTF8(*state->value);
  else
    *value = std::string();
  return true;
}

bool TextInputManagerTester::GetTextInputVkPolicy(
    ui::mojom::VirtualKeyboardPolicy* vk_policy) {
  DCHECK(observer_->text_input_manager());
  const ui::mojom::TextInputState* state =
      observer_->text_input_manager()->GetTextInputState();
  if (!state)
    return false;
  *vk_policy = state->vk_policy;
  return true;
}

bool TextInputManagerTester::GetTextInputVkVisibilityRequest(
    ui::mojom::VirtualKeyboardVisibilityRequest* last_vk_visibility_request) {
  DCHECK(observer_->text_input_manager());
  const ui::mojom::TextInputState* state =
      observer_->text_input_manager()->GetTextInputState();
  if (!state)
    return false;
  *last_vk_visibility_request = state->last_vk_visibility_request;
  return true;
}

bool TextInputManagerTester::GetTextInputShowImeIfNeeded(
    bool* show_ime_if_needed) {
  DCHECK(observer_->text_input_manager());
  const ui::mojom::TextInputState* state =
      observer_->text_input_manager()->GetTextInputState();
  if (!state)
    return false;
  *show_ime_if_needed = state->show_ime_if_needed;
  return true;
}

const RenderWidgetHostView* TextInputManagerTester::GetActiveView() {
  DCHECK(observer_->text_input_manager());
  return observer_->text_input_manager()->active_view_for_testing();
}

RenderWidgetHostView* TextInputManagerTester::GetUpdatedView() {
  return observer_->GetUpdatedView();
}

bool TextInputManagerTester::GetCurrentTextSelectionLength(size_t* length) {
  DCHECK(observer_->text_input_manager());

  if (!observer_->text_input_manager()->GetActiveWidget())
    return false;

  *length = observer_->text_input_manager()->GetTextSelection()->text().size();
  return true;
}

bool TextInputManagerTester::GetLastCompositionRangeLength(uint32_t* length) {
  if (!observer_->last_composition_range())
    return false;
  *length = observer_->last_composition_range()->length();
  return true;
}

bool TextInputManagerTester::IsTextInputStateChanged() {
  return observer_->text_input_state_changed();
}

TextInputManagerObserverBase::TextInputManagerObserverBase(
    content::WebContents* web_contents)
    : tester_(std::make_unique<TextInputManagerTester>(web_contents)),
      success_(false) {}

TextInputManagerObserverBase::~TextInputManagerObserverBase() = default;

void TextInputManagerObserverBase::Wait() {
  if (success_)
    return;

  base::RunLoop loop;
  quit_ = loop.QuitClosure();
  loop.Run();
}

void TextInputManagerObserverBase::OnSuccess() {
  success_ = true;
  if (quit_)
    std::move(quit_).Run();

  // By deleting |tester_| we make sure that the internal observer used in
  // content/ is removed from the observer list of TextInputManager.
  tester_.reset();
}

TextInputManagerValueObserver::TextInputManagerValueObserver(
    content::WebContents* web_contents,
    const std::string& expected_value)
    : TextInputManagerObserverBase(web_contents),
      expected_value_(expected_value) {
  tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
      &TextInputManagerValueObserver::VerifyValue, base::Unretained(this)));
}

void TextInputManagerValueObserver::VerifyValue() {
  std::string value;
  if (tester()->GetTextInputValue(&value) && expected_value_ == value)
    OnSuccess();
}

TextInputManagerTypeObserver::TextInputManagerTypeObserver(
    content::WebContents* web_contents,
    ui::TextInputType expected_type)
    : TextInputManagerObserverBase(web_contents),
      expected_type_(expected_type) {
  tester()->SetUpdateTextInputStateCalledCallback(base::BindRepeating(
      &TextInputManagerTypeObserver::VerifyType, base::Unretained(this)));
}

void TextInputManagerTypeObserver::VerifyType() {
  ui::TextInputType type =
      tester()->GetTextInputType(&type) ? type : ui::TEXT_INPUT_TYPE_NONE;
  if (expected_type_ == type)
    OnSuccess();
}

TestRenderWidgetHostViewDestructionObserver::
    TestRenderWidgetHostViewDestructionObserver(RenderWidgetHostView* view)
    : observer_(
          new InternalObserver(static_cast<RenderWidgetHostViewBase*>(view))) {}

TestRenderWidgetHostViewDestructionObserver::
    ~TestRenderWidgetHostViewDestructionObserver() {}

void TestRenderWidgetHostViewDestructionObserver::Wait() {
  observer_->Wait();
}

TextInputStateSender::TextInputStateSender(RenderWidgetHostView* view)
    : text_input_state_(ui::mojom::TextInputState::New()),
      view_(static_cast<RenderWidgetHostViewBase*>(view)) {}

TextInputStateSender::~TextInputStateSender() {}

void TextInputStateSender::Send() {
  if (view_)
    view_->TextInputStateChanged(*text_input_state_);
}

void TextInputStateSender::SetFromCurrentState() {
  if (view_) {
    const ui::mojom::TextInputState* state =
        RenderWidgetHostImpl::From(view_->GetRenderWidgetHost())
            ->delegate()
            ->GetTextInputManager()
            ->GetTextInputState();
    text_input_state_ = state->Clone();
  }
}

void TextInputStateSender::SetType(ui::TextInputType type) {
  text_input_state_->type = type;
}

void TextInputStateSender::SetMode(ui::TextInputMode mode) {
  text_input_state_->mode = mode;
}

void TextInputStateSender::SetFlags(int flags) {
  text_input_state_->flags = flags;
}

void TextInputStateSender::SetCanComposeInline(bool can_compose_inline) {
  text_input_state_->can_compose_inline = can_compose_inline;
}

void TextInputStateSender::SetShowVirtualKeyboardIfEnabled(
    bool show_ime_if_needed) {
  text_input_state_->show_ime_if_needed = show_ime_if_needed;
}

#if defined(USE_AURA)
void TextInputStateSender::SetLastPointerType(
    ui::EventPointerType last_pointer_type) {
  RenderWidgetHostViewAura* rwhva =
      static_cast<RenderWidgetHostViewAura*>(view_);
  rwhva->SetLastPointerType(last_pointer_type);
}
#endif

TestInputMethodObserver::TestInputMethodObserver() {}

TestInputMethodObserver::~TestInputMethodObserver() {}

// static
std::unique_ptr<TestInputMethodObserver> TestInputMethodObserver::Create(
    WebContents* web_contents) {
  std::unique_ptr<TestInputMethodObserver> observer;

#if defined(USE_AURA)
  RenderWidgetHostViewAura* view = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  observer = std::make_unique<InputMethodObserverAura>(view->GetInputMethod());
#endif
  return observer;
}

}  // namespace content
