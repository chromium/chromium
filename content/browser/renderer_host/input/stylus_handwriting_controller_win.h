// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_CONTROLLER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_CONTROLLER_WIN_H_

#include <ShellHandwriting.h>
#include <msctf.h>
#include <winerror.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <cstdint>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "content/common/content_export.h"

namespace base {
class ScopedClosureRunner;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace ui {
class TextInputClient;
}

namespace content {

// Encapsulates the Shell Handwriting API. Receives signals from the application
// to notify the Shell Handwriting API of relevant interactions, and forwards
// information back to the renderer process to perform hit testing and focus
// events.
class CONTENT_EXPORT StylusHandwritingControllerWin {
 public:
  // Callback invoked in response to the Shell Handwriting API
  // ITfHandwritingSink::FocusHandwritingTarget method.
  using OnFocusHandwritingTargetCallback =
      base::RepeatingCallback<void(const gfx::Rect& /*rect_in_screen*/,
                                   const gfx::Size& /*distance_threshold*/)>;

  // Sets `g_thread_manager_instance_for_testing` using the provided mocked
  // thread manager instance and initializes the controller instance. Resets the
  // state upon exiting the scope (e.g., on the test fixture tear down).
  [[nodiscard]] static base::ScopedClosureRunner InitializeForTesting(
      ITfThreadMgr* thread_manager);

  // Returns true if Shell Handwriting API is available and the bindings
  // have been successfully set up.
  static bool IsHandwritingAPIAvailable();

  // Initializes the controller singleton instance.
  static void Initialize();

  // Gets the controller instance if it was successfully initialized.
  // Nullptr otherwise.
  static StylusHandwritingControllerWin* GetInstance();

  static Microsoft::WRL::ComPtr<ITfThreadMgr> GetThreadManager();

  StylusHandwritingControllerWin(const StylusHandwritingControllerWin&) =
      delete;
  StylusHandwritingControllerWin& operator=(
      const StylusHandwritingControllerWin&) = delete;
  virtual ~StylusHandwritingControllerWin();

  // Notify the Shell Handwriting API about the intent to write. At this point,
  // we delegate the input processing to the API which starts inking. After
  // intent is confirmed, the API will request that focus is updated by calling
  // ITfHandwritingSink::FocusHandwritingTarget.
  void OnStartStylusWriting(OnFocusHandwritingTargetCallback callback,
                            uint32_t pointer_id,
                            uint64_t stroke_id,
                            ui::TextInputClient& text_input_client);

  // Signal the Handwriting API whether focus has been updated successfully and
  // may begin committing edits or collect character bounds for evaluating
  // gesture recognition using TSF/IME APIs.
  void OnFocusHandled(ui::TextInputClient& text_input_client);

  // Signal the Handwriting API that focus was not updated successfully and must
  // cancel the inking session.
  void OnFocusFailed(ui::TextInputClient& text_input_client);

 private:
  friend class base::NoDestructor<StylusHandwritingControllerWin>;

  // Binds interfaces and sets the global g_instance if the initialization is
  // successful.
  StylusHandwritingControllerWin();

  // Binds required API interfaces if available.
  void BindInterfaces();

  Microsoft::WRL::ComPtr<::ITfHandwriting> handwriting_;
  // Stores the current text input client where handwriting was initiated. Used
  // to filter out calls that come from other clients, e.g., when the focused
  // window changes after the stylus writing has started.
  base::WeakPtr<ui::TextInputClient> current_text_input_client_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_CONTROLLER_WIN_H_
