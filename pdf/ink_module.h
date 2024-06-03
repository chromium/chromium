// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_MODULE_H_
#define PDF_INK_MODULE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/ink/ink_stroke_input.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

class SkCanvas;

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}  // namespace blink

namespace gfx {
class PointF;
}  // namespace gfx

namespace chrome_pdf {

class InkInProgressStroke;
class InkStroke;
class PdfInkBrush;

class InkModule {
 public:
  class Client {
   public:
    virtual ~Client() = default;

    // Notifies the client that a stroke has finished drawing or erasing.
    virtual void InkStrokeFinished() {}

    // Returns the 0-based page index for the given `point` if it is on a
    // visible page, or -1 if `point` is not on a visible page.
    virtual int VisiblePageIndexFromPoint(const gfx::PointF& point) = 0;
  };

  explicit InkModule(Client& client);
  InkModule(const InkModule&) = delete;
  InkModule& operator=(const InkModule&) = delete;
  ~InkModule();

  bool enabled() const { return enabled_; }

  // Draws `ink_strokes_` and `ink_inputs_` into `canvas`.
  void Draw(SkCanvas& canvas);

  // Returns whether the event was handled or not.
  bool HandleInputEvent(const blink::WebInputEvent& event);

  // Returns whether the message was handled or not.
  bool OnMessage(const base::Value::Dict& message);

  // For testing only. Returns the current PDF ink brush used to draw strokes.
  const PdfInkBrush* GetPdfInkBrushForTesting() const;

 private:
  struct DrawingStrokeState {
    DrawingStrokeState();
    DrawingStrokeState(const DrawingStrokeState&) = delete;
    DrawingStrokeState& operator=(const DrawingStrokeState&) = delete;
    ~DrawingStrokeState();

    // The current brush to use for drawing strokes. Never null.
    std::unique_ptr<PdfInkBrush> ink_brush;

    std::optional<base::Time> ink_start_time;
    std::vector<InkStrokeInput> ink_inputs;
  };

  // No state, so just use a placeholder enum type.
  enum class EraserState { kIsEraser };

  // Returns whether the event was handled or not.
  bool OnMouseDown(const blink::WebMouseEvent& event);
  bool OnMouseUp(const blink::WebMouseEvent& event);
  bool OnMouseMove(const blink::WebMouseEvent& event);

  // Return values have the same semantics as OnMouse()* above.
  bool StartInkStroke(const gfx::PointF& position);
  bool ContinueInkStroke(const gfx::PointF& position);
  bool FinishInkStroke();

  // Return values have the same semantics as OnMouse*() above.
  bool StartEraseInkStroke(const gfx::PointF& position);
  bool ContinueEraseInkStroke(const gfx::PointF& position);
  bool FinishEraseInkStroke();

  void HandleAnnotationRedoMessage(const base::Value::Dict& message);
  void HandleAnnotationUndoMessage(const base::Value::Dict& message);
  void HandleSetAnnotationBrushMessage(const base::Value::Dict& message);
  void HandleSetAnnotationModeMessage(const base::Value::Dict& message);

  bool is_drawing_stroke() const {
    return absl::holds_alternative<DrawingStrokeState>(current_tool_state_);
  }
  bool is_erasing_stroke() const {
    return absl::holds_alternative<EraserState>(current_tool_state_);
  }
  const DrawingStrokeState& drawing_stroke_state() const {
    return absl::get<DrawingStrokeState>(current_tool_state_);
  }
  DrawingStrokeState& drawing_stroke_state() {
    return absl::get<DrawingStrokeState>(current_tool_state_);
  }

  // Converts `current_tool_state_` into an InkInProgressStroke. Requires
  // `current_tool_state_` to hold a `DrawingStrokeState`. If there is no
  // `DrawingStrokeState`, or the state currently has no inputs, then return
  // nullptr.
  std::unique_ptr<InkInProgressStroke> CreateInProgressStrokeFromInputs() const;

  const raw_ref<Client> client_;

  bool enabled_ = false;

  // The state of the current tool that is in use.
  absl::variant<DrawingStrokeState, EraserState> current_tool_state_;

  // The strokes that have been completed.
  std::vector<std::unique_ptr<InkStroke>> ink_strokes_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_MODULE_H_
