// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_FRAME_INPUT_HANDLER_IMPL_H_
#define CONTENT_RENDERER_INPUT_FRAME_INPUT_HANDLER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class MainThreadEventQueue;

// This class provides an implementation of FrameInputHandler mojo interface.
// When a compositor thread is being used in the renderer the mojo channel
// is bound on the compositor thread. Method calls, and events received on the
// compositor thread are then placed in the MainThreadEventQueue for
// the associated RenderWidget. This is done as to ensure that input related
// messages and events that are handled on the compositor thread aren't
// executed before other input events that need to be processed on the
// main thread. ie. Since some messages flow to the compositor thread
// all input needs to flow there so the ordering of events is kept in sequence.
//
// eg. (B = Browser, CT = Compositor Thread, MT = Main Thread)
//   B sends MouseEvent
//   B sends Copy message
//   CT receives MouseEvent (CT might do something with the MouseEvent)
//   CT places MouseEvent in MainThreadEventQueue
//   CT receives Copy message (CT has no use for the Copy message)
//   CT places Copy message in MainThreadEventQueue
//   MT receives MouseEvent
//   MT receives Copy message
//
// When a compositor thread isn't used the mojo channel is just bound
// on the main thread and messages are handled right away.
class CONTENT_EXPORT FrameInputHandlerImpl : public mojom::FrameInputHandler {
 public:
  static void CreateMojoService(
      base::WeakPtr<RenderFrameImpl> render_frame,
      mojo::PendingReceiver<mojom::FrameInputHandler> receiver);

  void SetCompositionFromExistingText(
      int32_t start,
      int32_t end,
      const std::vector<ui::ImeTextSpan>& ime_text_spans) override;
  void ExtendSelectionAndDelete(int32_t before, int32_t after) override;
  void DeleteSurroundingText(int32_t before, int32_t after) override;
  void DeleteSurroundingTextInCodePoints(int32_t before,
                                         int32_t after) override;
  void SetEditableSelectionOffsets(int32_t start, int32_t end) override;
  void ExecuteEditCommand(const std::string& command,
                          const base::Optional<base::string16>& value) override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void CopyToFindPboard() override;
  void Paste() override;
  void PasteAndMatchStyle() override;
  void Replace(const base::string16& word) override;
  void ReplaceMisspelling(const base::string16& word) override;
  void Delete() override;
  void SelectAll() override;
  void CollapseSelection() override;
  void SelectRange(const gfx::Point& base, const gfx::Point& extent) override;
#if defined(OS_ANDROID)
  void SelectWordAroundCaret(SelectWordAroundCaretCallback callback) override;
#endif  // defined(OS_ANDROID)
  void AdjustSelectionByCharacterOffset(
      int32_t start,
      int32_t end,
      blink::mojom::SelectionMenuBehavior selection_menu_behavior) override;
  void MoveRangeSelectionExtent(const gfx::Point& extent) override;
  void ScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect) override;
  void MoveCaret(const gfx::Point& point) override;
  void GetWidgetInputHandler(
      mojo::PendingAssociatedReceiver<mojom::WidgetInputHandler> receiver,
      mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) override;

 private:
  ~FrameInputHandlerImpl() override;
  enum class UpdateState { kNone, kIsPasting, kIsSelectingRange };

  class HandlingState {
   public:
    HandlingState(const base::WeakPtr<RenderFrameImpl>& render_frame,
                  UpdateState state);
    ~HandlingState();

   private:
    base::WeakPtr<RenderFrameImpl> render_frame_;
    bool original_select_range_value_;
    bool original_pasting_value_;
  };

  FrameInputHandlerImpl(
      base::WeakPtr<RenderFrameImpl> render_frame,
      mojo::PendingReceiver<mojom::FrameInputHandler> receiver);

  void RunOnMainThread(base::OnceClosure closure);
  void BindNow(mojo::PendingReceiver<mojom::FrameInputHandler> receiver);
  void ExecuteCommandOnMainThread(const std::string& command,
                                  UpdateState state);
  void Release();

  mojo::Receiver<mojom::FrameInputHandler> receiver_{this};

  // |render_frame_| should only be accessed on the main thread. Use
  // GetRenderFrame so that it will DCHECK this for you.
  base::WeakPtr<RenderFrameImpl> render_frame_;

  scoped_refptr<MainThreadEventQueue> input_event_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtr<FrameInputHandlerImpl> weak_this_;
  base::WeakPtrFactory<FrameInputHandlerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FrameInputHandlerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_FRAME_INPUT_HANDLER_IMPL_H_
