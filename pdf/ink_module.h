// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_MODULE_H_
#define PDF_INK_MODULE_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/ink/ink_affine_transform.h"
#include "pdf/ink/ink_stroke_input.h"
#include "pdf/page_orientation.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

class SkCanvas;

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}  // namespace blink

namespace chrome_pdf {

class InkInProgressStroke;
class InkStroke;
class PdfInkBrush;

class InkModule {
 public:
  using InkStrokeInputPoints = std::vector<gfx::PointF>;

  // Each page of a document can have many strokes.  The input points for each
  // stroke are restricted to just one page.
  using PageInkStrokeInputPoints = std::vector<InkStrokeInputPoints>;

  // Mapping of a 0-based page index to the input points that make up the ink
  // strokes for that page.
  using DocumentInkStrokeInputPointsMap =
      std::map<int, PageInkStrokeInputPoints>;

  using RenderTransformCallback =
      base::RepeatingCallback<void(const InkAffineTransform& transform)>;

  class Client {
   public:
    virtual ~Client() = default;

    // Gets the current page orientation.
    virtual PageOrientation GetOrientation() const = 0;

    // Gets the current scaled and rotated rectangle area of the page in CSS
    // screen coordinates for the 0-based page index.  Must be non-empty for any
    // non-negative index returned from `VisiblePageIndexFromPoint()`.
    virtual gfx::Rect GetPageContentsRect(int index) = 0;

    // Gets the offset within the rendering viewport to where the page images
    // will be drawn.  Since the offset is a location within the viewport, it
    // must always contain non-negative values.  Values are in scaled CSS
    // screen coordinates, where the amount of scaling matches that of
    // `GetZoom()`.  The page orientation does not apply to the viewport.
    virtual gfx::Vector2dF GetViewportOriginOffset() = 0;

    // Gets current zoom factor.
    virtual float GetZoom() const = 0;

    // Notifies the client that a stroke has finished drawing or erasing.
    virtual void InkStrokeFinished() {}

    // Notifies the client to invalidate the `rect`.  Coordinates are
    // screen-based, based on the same viewport origin that was used to specify
    // the `blink::WebMouseEvent` positions during stroking.
    virtual void Invalidate(const gfx::Rect& rect) {}

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

  // For testing only. Returns the input positions used for the stroke.
  DocumentInkStrokeInputPointsMap GetInkStrokesInputPositionsForTesting() const;

  // For testing only. Provide a callback to use whenever the rendering
  // transform is determined for `Draw()`.
  void SetDrawRenderTransformCallbackForTesting(
      RenderTransformCallback callback);

 private:
  struct DrawingStrokeState {
    DrawingStrokeState();
    DrawingStrokeState(const DrawingStrokeState&) = delete;
    DrawingStrokeState& operator=(const DrawingStrokeState&) = delete;
    ~DrawingStrokeState();

    // The current brush to use for drawing strokes. Never null.
    std::unique_ptr<PdfInkBrush> ink_brush;

    std::optional<base::Time> ink_start_time;

    // The 0-based page index which is currently being stroked.
    int ink_page_index = -1;

    // The event position for the last ink input.  Coordinates match the
    // screen-based position that are provided during stroking from
    // `blink::WebMouseEvent` positions.  Used after stroking has already
    // started, to support invalidation.
    gfx::PointF ink_input_last_event_position;

    // The points that make up the current stroke. Coordinates for each
    // `InkStrokeInput` are stored in a canonical format specified in
    // pdf_ink_transform.h.
    std::vector<InkStrokeInput> ink_inputs;
  };

  // Each page of a document can have many strokes.  Each stroke is restricted
  // to just one page.
  using PageInkStrokes = std::vector<std::unique_ptr<InkStroke>>;

  // Mapping of a 0-based page index to the ink strokes for that page.
  using DocumentInkStrokesMap = std::map<int, PageInkStrokes>;

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

  // The strokes that have been completed.  Coordinates for each stroke are
  // stored in a canonical format specified in pdf_ink_transform.h.
  DocumentInkStrokesMap ink_strokes_;

  RenderTransformCallback draw_render_transform_callback_for_testing_;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_MODULE_H_
