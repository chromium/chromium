// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEXT_INPUT_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_TEXT_INPUT_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/test/test_utils.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/mojom/virtual_keyboard_types.mojom.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"

#if defined(USE_AURA)
#include "ui/events/event_constants.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/public/test/fake_local_frame.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#endif

namespace gfx {
class Range;
}

namespace ui {
struct ImeTextSpan;
}

namespace content {

class RenderFrameHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class WebContents;

// Returns the |TextInputState.type| from the TextInputManager owned by
// |web_contents|.
ui::TextInputType GetTextInputTypeFromWebContents(WebContents* web_contents);

const ui::mojom::TextInputState* GetTextInputStateFromWebContents(
    WebContents* web_contents);

// This method returns true if |view| is registered in the TextInputManager that
// is owned by |web_contents|. If that is the case, the value of |type| will be
// the |TextInputState.type| corresponding to the |view|. Returns false if
// |view| is not registered.
bool GetTextInputTypeForView(WebContents* web_contents,
                             RenderWidgetHostView* view,
                             ui::TextInputType* type);

// This method returns the number of RenderWidgetHostViews which are currently
// registered with the TextInputManager that is owned by |web_contents|.
size_t GetRegisteredViewsCountFromTextInputManager(WebContents* web_contents);

// Returns the RWHV corresponding to the frame with a focused <input> within the
// given WebContents.
RenderWidgetHostView* GetActiveViewFromWebContents(WebContents* web_contents);

// This method will send a request for an immediate update on composition range
// from TextInputManager's active widget corresponding to the |web_contents|.
// This function will return false if the request is not successfully sent;
// either due to missing TextInputManager or lack of an active widget.
bool RequestCompositionInfoFromActiveWidget(WebContents* web_contents);

// Returns true if |frame| has a focused editable element.
bool DoesFrameHaveFocusedEditableElement(RenderFrameHost* frame);

// Sends a request to the RenderWidget corresponding to |rwh| to commit the
// given |text|.
void SendImeCommitTextToWidget(
    RenderWidgetHost* rwh,
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int relative_cursor_pos);

// Sends a request to RenderWidget corresponding to |rwh| to set the given
// composition text and update the corresponding IME params.
void SendImeSetCompositionTextToWidget(
    RenderWidgetHost* rwh,
    const std::u16string& text,
    const std::vector<ui::ImeTextSpan>& ime_text_spans,
    const gfx::Range& replacement_range,
    int selection_start,
    int selection_end);

// Triggers the TextInputStateChanged event on the RenderWidget corresponding to
// |rwh|.
void SendTextInputStateChangedToWidget(RenderWidgetHost* rwh,
                                       ui::mojom::TextInputStatePtr state);

// Immediately destroys the RenderWidgetHost corresponding to the local root
// which is identified by the given process ID and RenderFrameHost routing ID.
bool DestroyRenderWidgetHost(int32_t process_id, int32_t local_root_routing_id);

// This class provides the necessary API for accessing the state of and also
// observing the TextInputManager for WebContents.
class TextInputManagerTester {
 public:
  TextInputManagerTester(WebContents* web_contents);

  TextInputManagerTester(const TextInputManagerTester&) = delete;
  TextInputManagerTester& operator=(const TextInputManagerTester&) = delete;

  virtual ~TextInputManagerTester();

  // Sets a callback which is invoked when a RWHV calls UpdateTextInputState
  // on the TextInputManager which is being observed.
  void SetUpdateTextInputStateCalledCallback(base::RepeatingClosure callback);

  // Sets a callback which is invoked when a RWHV calls SelectionBoundsChanged
  // on the TextInputManager which is being observed.
  void SetOnSelectionBoundsChangedCallback(base::RepeatingClosure callback);

  // Sets a callback which is invoked when a RWHV calls
  // ImeCompositionRangeChanged on the TextInputManager that is being observed.
  void SetOnImeCompositionRangeChangedCallback(base::RepeatingClosure callback);

  // Sets a callback which is invoked when a RWHV calls SelectionChanged on the
  // TextInputManager which is being observed.
  void SetOnTextSelectionChangedCallback(base::RepeatingClosure callback);

  // Returns true if there is a focused <input> and populates |type| with
  // |TextInputState.type| of the TextInputManager.
  bool GetTextInputType(ui::TextInputType* type);

  // Returns true if there is a focused <input> and populates |value| with
  // |TextInputState.value| of the TextInputManager.
  bool GetTextInputValue(std::string* value);

  // Returns true if there is a focused editable element and populates
  // |vk_policy| with |TextInputState.vk_policy| of the TextInputManager.
  bool GetTextInputVkPolicy(ui::mojom::VirtualKeyboardPolicy* vk_policy);

  // Returns true if there is a focused editable element and populates
  // |last_vk_visibility_request| with
  // |TextInputState.last_vk_visibility_request| of the TextInputManager.
  bool GetTextInputVkVisibilityRequest(
      ui::mojom::VirtualKeyboardVisibilityRequest* last_vk_visibility_request);

  // Returns true if there is a focused editable element tapped by the user
  //  and populates |show_ime_if_needed| with
  // |TextInputState.show_ime_if_needed| of the TextInputManager.
  bool GetTextInputShowImeIfNeeded(bool* show_ime_if_needed);

  // Returns true if there is a focused <input> and populates |length| with the
  // length of the selected text range in the focused view.
  bool GetCurrentTextSelectionLength(size_t* length);

  // This method sets |output| to the last value of composition range length
  // reported by a renderer corresponding to WebContents. If no such update have
  // been received, the method will leave |output| untouched and returns false.
  // Returning true means an update has been received and the value of |output|
  // has been updated accordingly.
  bool GetLastCompositionRangeLength(uint32_t* output);

  // Returns the RenderWidgetHostView with a focused <input> element or nullptr
  // if none exists.
  const RenderWidgetHostView* GetActiveView();

  // Returns the RenderWidgetHostView which has most recently updated any of its
  // state (e.g., TextInputState or otherwise).
  RenderWidgetHostView* GetUpdatedView();

  // Returns true if a call to TextInputManager::UpdateTextInputState has led
  // to a change in TextInputState (since the time the observer has been
  // created).
  bool IsTextInputStateChanged();

 private:
  // The actual internal observer of the TextInputManager.
  class InternalObserver;

  std::unique_ptr<InternalObserver> observer_;
};

// TextInputManager Observers

// A base class for observing the TextInputManager owned by the given
// WebContents. Subclasses can observe the TextInputManager for different
// changes. The class wraps a public tester which accepts callbacks that
// are run after specific changes in TextInputManager. Different observers can
// be subclassed from this by providing their specific callback methods.
class TextInputManagerObserverBase {
 public:
  explicit TextInputManagerObserverBase(content::WebContents* web_contents);

  virtual ~TextInputManagerObserverBase();

  TextInputManagerObserverBase(const TextInputManagerObserverBase&) = delete;
  TextInputManagerObserverBase& operator=(const TextInputManagerObserverBase&) =
      delete;

  // Wait for derived class's definition of success.
  void Wait();

  bool success() const { return success_; }

 protected:
  content::TextInputManagerTester* tester() { return tester_.get(); }

  void OnSuccess();

 private:
  std::unique_ptr<content::TextInputManagerTester> tester_;
  bool success_;
  base::OnceClosure quit_;
};

// This class observes TextInputManager for changes in |TextInputState.value|.
class TextInputManagerValueObserver : public TextInputManagerObserverBase {
 public:
  TextInputManagerValueObserver(content::WebContents* web_contents,
                                const std::string& expected_value);

  TextInputManagerValueObserver(const TextInputManagerValueObserver&) = delete;
  TextInputManagerValueObserver& operator=(
      const TextInputManagerValueObserver&) = delete;

 private:
  void VerifyValue();
  const std::string expected_value_;
};

// This class observes TextInputManager for changes in |TextInputState.type|.
class TextInputManagerTypeObserver : public TextInputManagerObserverBase {
 public:
  TextInputManagerTypeObserver(content::WebContents* web_contents,
                               ui::TextInputType expected_type);

  TextInputManagerTypeObserver(const TextInputManagerTypeObserver&) = delete;
  TextInputManagerTypeObserver& operator=(const TextInputManagerTypeObserver&) =
      delete;

 private:
  void VerifyType();

  const ui::TextInputType expected_type_;
};

// This class observes the lifetime of a RenderWidgetHostView.
class TestRenderWidgetHostViewDestructionObserver {
 public:
  TestRenderWidgetHostViewDestructionObserver(RenderWidgetHostView* view);

  TestRenderWidgetHostViewDestructionObserver(
      const TestRenderWidgetHostViewDestructionObserver&) = delete;
  TestRenderWidgetHostViewDestructionObserver& operator=(
      const TestRenderWidgetHostViewDestructionObserver&) = delete;

  virtual ~TestRenderWidgetHostViewDestructionObserver();

  // Waits for the RWHV which is being observed to get destroyed.
  void Wait();

 private:
  // The actual internal observer of RenderWidgetHostViewBase.
  class InternalObserver;

  std::unique_ptr<InternalObserver> observer_;
};

// Helper class to create TextInputState structs on the browser side and send it
// to the given RenderWidgetHostView. This class can be used for faking changes
// in TextInputState for testing on the browser side.
class TextInputStateSender {
 public:
  explicit TextInputStateSender(RenderWidgetHostView* view);

  TextInputStateSender(const TextInputStateSender&) = delete;
  TextInputStateSender& operator=(const TextInputStateSender&) = delete;

  virtual ~TextInputStateSender();

  void Send();

  void SetFromCurrentState();

  // The required setter methods. These setter methods can be used to call
  // RenderWidgetHostViewBase::TextInputStateChanged with fake, customized
  // TextInputState. Used for unit-testing on the browser side.
  void SetType(ui::TextInputType type);
  void SetMode(ui::TextInputMode mode);
  void SetFlags(int flags);
  void SetCanComposeInline(bool can_compose_inline);
  void SetShowVirtualKeyboardIfEnabled(bool show_ime_if_needed);
#if defined(USE_AURA)
  void SetLastPointerType(ui::EventPointerType last_pointer_type);
#endif

 private:
  ui::mojom::TextInputStatePtr text_input_state_;
  const raw_ptr<RenderWidgetHostViewBase> view_;
};

// This class is intended to observe the InputMethod.
class TestInputMethodObserver {
 public:
  // static
  // Creates and returns a platform specific implementation of an
  // InputMethodObserver.
  static std::unique_ptr<TestInputMethodObserver> Create(
      WebContents* web_contents);

  virtual ~TestInputMethodObserver();

  virtual ui::TextInputType GetTextInputTypeFromClient() = 0;

  virtual void SetOnVirtualKeyboardVisibilityChangedIfEnabledCallback(
      const base::RepeatingCallback<void(bool)>& callback) = 0;

 protected:
  TestInputMethodObserver();
};

#if BUILDFLAG(IS_MAC)
// Helper class to test LocalFrame::GetStringForRange.
class TextInputTestLocalFrame : public FakeLocalFrame {
 public:
  TextInputTestLocalFrame();

  TextInputTestLocalFrame(const TextInputTestLocalFrame&) = delete;
  TextInputTestLocalFrame& operator=(const TextInputTestLocalFrame&) = delete;

  ~TextInputTestLocalFrame() override;

  void SetUp(content::RenderFrameHost* render_frame_host);
  void WaitForGetStringForRange();
  // Sets a callback for the string for range message arriving from the
  // renderer. The callback is invoked before that of TextInputClientMac.
  void SetStringForRangeCallback(base::RepeatingClosure callback);

  std::string GetStringFromRange() { return string_from_range_; }
  void SetStringFromRange(std::string string_from_range) {
    string_from_range_ = string_from_range;
  }

  // blink::mojom::LocalFrame:
  void GetStringForRange(const gfx::Range& range,
                         GetStringForRangeCallback callback) override;

 private:
  base::OnceClosure quit_closure_;
  base::RepeatingClosure string_for_range_callback_;
  std::string string_from_range_;
  mojo::AssociatedRemote<blink::mojom::LocalFrame> local_frame_;
};

// Requests the |tab_view| for the definition of the word identified by the
// given selection range. |range| identifies a word in the focused
// RenderWidgetHost underneath |tab_view| which may be different than the
// RenderWidgetHost corresponding to |tab_view|.
void AskForLookUpDictionaryForRange(RenderWidgetHostView* tab_view,
                                    const gfx::Range& range);

#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEXT_INPUT_TEST_UTILS_H_
