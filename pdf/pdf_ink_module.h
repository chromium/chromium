// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_MODULE_H_
#define PDF_PDF_INK_MODULE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/ink/ink_affine_transform.h"
#include "pdf/ink/ink_stroke_input.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_ink_undo_redo_model.h"
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

class PdfInkModule {
 public:
  using StrokeInputPoints = std::vector<gfx::PointF>;

  // Each page of a document can have many strokes.  The input points for each
  // stroke are restricted to just one page.
  using PageStrokeInputPoints = std::vector<StrokeInputPoints>;

  // Mapping of a 0-based page index to the input points that make up the
  // strokes for that page.
  using DocumentStrokeInputPointsMap = std::map<int, PageStrokeInputPoints>;

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
    virtual void StrokeFinished() {}

    // Notifies the client to invalidate the `rect`.  Coordinates are
    // screen-based, based on the same viewport origin that was used to specify
    // the `blink::WebMouseEvent` positions during stroking.
    virtual void Invalidate(const gfx::Rect& rect) {}

    // Returns whether the page at `index` is visible or not.
    virtual bool IsPageVisible(int index) = 0;

    // Returns the 0-based page index for the given `point` if it is on a
    // visible page, or -1 if `point` is not on a visible page.
    virtual int VisiblePageIndexFromPoint(const gfx::PointF& point) = 0;
  };

  explicit PdfInkModule(Client& client);
  PdfInkModule(const PdfInkModule&) = delete;
  PdfInkModule& operator=(const PdfInkModule&) = delete;
  ~PdfInkModule();

  bool enabled() const { return enabled_; }

  // Draws `strokes_` and `inputs_` into `canvas`.
  void Draw(SkCanvas& canvas);

  // Returns whether the event was handled or not.
  bool HandleInputEvent(const blink::WebInputEvent& event);

  // Returns whether the message was handled or not.
  bool OnMessage(const base::Value::Dict& message);

  // For testing only. Returns the current `PdfInkBrush` used to draw strokes,
  // or nullptr if there is no brush.
  const PdfInkBrush* GetPdfInkBrushForTesting() const;

  // For testing only. Returns the current eraser size, or nullopt if the
  // eraser is not in use.
  std::optional<float> GetEraserSizeForTesting() const;

  // For testing only. Returns the (visible) input positions used for all
  // strokes in the document.
  DocumentStrokeInputPointsMap GetStrokesInputPositionsForTesting() const;
  DocumentStrokeInputPointsMap GetVisibleStrokesInputPositionsForTesting()
      const;

  // For testing only. Provide a callback to use whenever the rendering
  // transform is determined for `Draw()`.
  void SetDrawRenderTransformCallbackForTesting(
      RenderTransformCallback callback);

 private:
  using StrokeInputSegment = std::vector<InkStrokeInput>;

  struct DrawingStrokeState {
    DrawingStrokeState();
    DrawingStrokeState(const DrawingStrokeState&) = delete;
    DrawingStrokeState& operator=(const DrawingStrokeState&) = delete;
    ~DrawingStrokeState();

    // The current brush to use for drawing strokes. Never null.
    std::unique_ptr<PdfInkBrush> brush;

    std::optional<base::Time> start_time;

    // The 0-based page index which is currently being stroked.
    int page_index = -1;

    // The event position for the last input.  Coordinates match the
    // screen-based position that are provided during stroking from
    // `blink::WebMouseEvent` positions.  Used after stroking has already
    // started, to support invalidation.
    std::optional<gfx::PointF> input_last_event_position;

    // The points that make up the current stroke, divided into
    // StrokeInputSegments.  A new segment will be necessary each time the input
    // leaves the page during collection and then returns back into the original
    // starting page.  The coordinates added into each segment are stored in a
    // canonical format specified in pdf_ink_transform.h.
    std::vector<StrokeInputSegment> inputs;
  };

  // A stroke that has been completed, its ID, and whether it should be drawn
  // or not.
  struct FinishedStrokeState {
    FinishedStrokeState(std::unique_ptr<InkStroke> stroke, size_t id);
    FinishedStrokeState(const FinishedStrokeState&) = delete;
    FinishedStrokeState& operator=(const FinishedStrokeState&) = delete;
    FinishedStrokeState(FinishedStrokeState&&) noexcept;
    FinishedStrokeState& operator=(FinishedStrokeState&&) noexcept;
    ~FinishedStrokeState();

    // Coordinates for each stroke are stored in a canonical format specified in
    // pdf_ink_transform.h.
    std::unique_ptr<InkStroke> stroke;

    // A unique ID to identify this stroke.
    size_t id;

    bool should_draw = true;
  };

  // Each page of a document can have many strokes.  Each stroke is restricted
  // to just one page.
  // The elements are stored with IDs in an increasing order.
  using PageStrokes = std::vector<FinishedStrokeState>;

  // Mapping of a 0-based page index to the strokes for that page.
  using DocumentStrokesMap = std::map<int, PageStrokes>;

  class StrokeIdGenerator {
   public:
    StrokeIdGenerator();
    ~StrokeIdGenerator();

    // Returns an available ID and advance the next available ID internally.
    size_t GetIdAndAdvance();

    void ResetIdTo(size_t id);

   private:
    // The next available ID for use in FinishedStrokeState.
    size_t next_stroke_id_ = 0;
  };

  struct EraserState {
    bool erasing = false;
    bool did_erase_strokes = false;
    float eraser_size = 0;
  };

  // Returns whether the event was handled or not.
  bool OnMouseDown(const blink::WebMouseEvent& event);
  bool OnMouseUp(const blink::WebMouseEvent& event);
  bool OnMouseMove(const blink::WebMouseEvent& event);

  // Return values have the same semantics as OnMouse()* above.
  bool StartStroke(const gfx::PointF& position);
  bool ContinueStroke(const gfx::PointF& position);
  bool FinishStroke();

  // Return values have the same semantics as OnMouse*() above.
  bool StartEraseStroke(const gfx::PointF& position);
  bool ContinueEraseStroke(const gfx::PointF& position);
  bool FinishEraseStroke();

  // Shared code for the Erase methods above. Returns if stroke(s) got erased or
  // not.
  bool EraseHelper(const gfx::PointF& position, int page_index);

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
  const EraserState& erasing_stroke_state() const {
    return absl::get<EraserState>(current_tool_state_);
  }
  EraserState& erasing_stroke_state() {
    return absl::get<EraserState>(current_tool_state_);
  }

  // Converts `current_tool_state_` into segments of `InkInProgressStroke`.
  // Requires `current_tool_state_` to hold a `DrawingStrokeState`. If there is
  // no `DrawingStrokeState`, or the state currently has no inputs, then the
  // segments will be empty.
  std::vector<std::unique_ptr<InkInProgressStroke>>
  CreateInProgressStrokeSegmentsFromInputs() const;

  // Wrapper around EventPositionToCanonicalPosition(). `page_index` is the page
  // that `position` is on. The page must be visible.
  gfx::PointF ConvertEventPositionToCanonicalPosition(
      const gfx::PointF& position,
      int page_index);

  void ApplyUndoRedoCommands(const PdfInkUndoRedoModel::Commands& commands);
  void ApplyUndoRedoCommandsHelper(std::set<size_t> ids, bool should_draw);

  void ApplyUndoRedoDiscards(
      const PdfInkUndoRedoModel::DiscardedDrawCommands& discards);

  const raw_ref<Client> client_;

  bool enabled_ = false;

  // Generates IDs for use in FinishedStrokeState and PdfInkUndoRedoModel.
  StrokeIdGenerator stroke_id_generator_;

  // The state of the current tool that is in use.
  absl::variant<DrawingStrokeState, EraserState> current_tool_state_;

  // The state of the strokes that have been completed.
  DocumentStrokesMap strokes_;

  PdfInkUndoRedoModel undo_redo_model_;

  RenderTransformCallback draw_render_transform_callback_for_testing_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_MODULE_H_
