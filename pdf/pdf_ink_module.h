// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_MODULE_H_
#define PDF_PDF_INK_MODULE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/page_orientation.h"
#include "pdf/pdf_ink_annotation_mode.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdf_ink_ids.h"
#include "pdf/pdf_ink_undo_redo_model.h"
#include "pdf/ui/thumbnail.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/partitioned_mesh.h"
#include "third_party/ink/src/ink/rendering/skia/native/skia_renderer.h"
#include "third_party/ink/src/ink/strokes/in_progress_stroke.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

class SkCanvas;

namespace blink {
class WebKeyboardEvent;
class WebInputEvent;
class WebMouseEvent;
class WebPointerProperties;
class WebTouchEvent;
}  // namespace blink

namespace chrome_pdf {

class PdfInkModuleClient;

class PdfInkModule {
 public:
  explicit PdfInkModule(PdfInkModuleClient& client);
  PdfInkModule(const PdfInkModule&) = delete;
  PdfInkModule& operator=(const PdfInkModule&) = delete;
  ~PdfInkModule();

  bool enabled() const { return mode_ != InkAnnotationMode::kOff; }

  // Returns whether the text selection change event should be blocked to
  // prevent modifying the clipboard content.
  bool ShouldBlockTextSelectionChanged();

  // Determines if there are any in-progress inputs to be drawn.
  bool HasInputsToDraw() const;

  // Draws any in-progress inputs into `canvas`.  Must either be in text
  // highlighting state or in drawing stroke state with non-empty
  // `drawing_stroke_state().inputs`.
  void Draw(SkCanvas& canvas);

  // Generates a thumbnail of `thumbnail_size` for the page at `page_index`
  // using DrawThumbnail(). Sends the result to the WebUI if successful.
  // Otherwise, do not send anything to the WebUI.
  // `thumbnail_size` must be non-empty.
  void GenerateAndSendInkThumbnail(int page_index,
                                   const gfx::Size& thumbnail_size);

  // Returns whether the event was handled or not.
  bool HandleInputEvent(const blink::WebInputEvent& event);

  // Returns whether the message was handled or not.
  bool OnMessage(const base::Value::Dict& message);

  // Informs PdfInkModule that the plugin geometry changed.
  void OnGeometryChanged();

  // For testing only. Returns the current `PdfInkBrush` used to draw strokes,
  // or nullptr if there is no brush because `PdfInkModule` is not in the
  // drawing state.
  const PdfInkBrush* GetPdfInkBrushForTesting() const;

 private:
  friend class PdfInkModuleStrokeTest;
  FRIEND_TEST_ALL_PREFIXES(PdfInkModuleTest, HandleSetAnnotationModeMessage);

  // A stroke that has been completed, its ID, and whether it should be drawn
  // or not.
  struct FinishedStrokeState {
    FinishedStrokeState(ink::Stroke stroke, InkStrokeId id);
    FinishedStrokeState(const FinishedStrokeState&) = delete;
    FinishedStrokeState& operator=(const FinishedStrokeState&) = delete;
    FinishedStrokeState(FinishedStrokeState&&) noexcept;
    FinishedStrokeState& operator=(FinishedStrokeState&&) noexcept;
    ~FinishedStrokeState();

    // Coordinates for each stroke are stored in a canonical format specified in
    // pdf_ink_transform.h.
    ink::Stroke stroke;

    // A unique ID to identify this stroke.
    InkStrokeId id;

    bool should_draw = true;
  };

  // Each page of a document can have many strokes.  Each stroke is restricted
  // to just one page.
  // The elements are stored with IDs in an increasing order.
  using PageStrokes = std::vector<FinishedStrokeState>;

  // Mapping of a 0-based page index to the strokes for that page.
  using DocumentStrokesMap = std::map<int, PageStrokes>;

  // A shape that was loaded from a "V2" path from the PDF itself, its ID, and
  // whether it should be drawn or not.
  struct LoadedV2ShapeState {
    LoadedV2ShapeState(ink::PartitionedMesh shape, InkModeledShapeId id);
    LoadedV2ShapeState(const LoadedV2ShapeState&) = delete;
    LoadedV2ShapeState& operator=(const LoadedV2ShapeState&) = delete;
    LoadedV2ShapeState(LoadedV2ShapeState&&) noexcept;
    LoadedV2ShapeState& operator=(LoadedV2ShapeState&&) noexcept;
    ~LoadedV2ShapeState();

    // Coordinates for each shape are stored in a canonical format specified in
    // pdf_ink_transform.h.
    ink::PartitionedMesh shape;

    // A unique ID to identify this shape.
    InkModeledShapeId id;

    bool should_draw = true;
  };

  // Like PageStrokes, but for shapes created from "V2" paths in the PDF.
  using PageV2InkPathShapes = std::vector<LoadedV2ShapeState>;

  // Like DocumentStrokesMap, but for PageV2InkPathShapes.
  using DocumentV2InkPathShapesMap = std::map<int, PageV2InkPathShapes>;

  struct EventDetails {
    // The event position.  Coordinates match the screen-based position that
    // are provided during stroking from `blink::WebMouseEvent` positions.
    gfx::PointF position;

    // The event time.
    base::TimeTicks timestamp;

    // The type of tool used to generate the input.
    ink::StrokeInput::ToolType tool_type = ink::StrokeInput::ToolType::kUnknown;
  };

  struct DrawingStrokeState {
    DrawingStrokeState();
    DrawingStrokeState(const DrawingStrokeState&) = delete;
    DrawingStrokeState& operator=(const DrawingStrokeState&) = delete;
    ~DrawingStrokeState();

    // The current brush type to use for drawing strokes.
    PdfInkBrush::Type brush_type;

    std::optional<base::TimeTicks> start_time;

    // The 0-based page index which is currently being stroked.
    int page_index = -1;

    // Details from the last input.  Used after stroking has already started,
    // for invalidation and for extrapolating where a stroke crosses the page
    // boundary.  Also used to compensate for missed events, when an end event
    // was consumed by a different view and this is detected afterwards when
    // PdfInkModule finally sees input events again.
    std::optional<EventDetails> input_last_event;

    // The points that make up the current stroke, divided into segments.
    // A new segment will be necessary each time the input leaves the page
    // during collection and then returns back into the original starting page.
    // The coordinates added into each segment are stored in a canonical format
    // specified in pdf_ink_transform.h.
    std::vector<ink::StrokeInputBatch> inputs;
  };

  class StrokeIdGenerator {
   public:
    StrokeIdGenerator();
    ~StrokeIdGenerator();

    // Returns an available ID and advance the next available ID internally.
    InkStrokeId GetIdAndAdvance();

    void ResetIdTo(InkStrokeId id);

   private:
    // The next available ID for use in FinishedStrokeState.
    InkStrokeId next_stroke_id_ = InkStrokeId(0);
  };

  struct EraserState {
    EraserState();
    EraserState(const EraserState&) = delete;
    EraserState& operator=(const EraserState&) = delete;
    ~EraserState();

    bool erasing = false;
    base::flat_set<int> page_indices_with_stroke_erasures;
    base::flat_set<int> page_indices_with_partitioned_mesh_erasures;

    // The event position for the last input, similar to what is stored in
    // `DrawingStrokeState` for compensating for missed input events.
    std::optional<gfx::PointF> input_last_event_position;

    // The type of tool used to generate the input.
    ink::StrokeInput::ToolType tool_type;
  };

  struct TextHighlightState {
    TextHighlightState();
    TextHighlightState(const TextHighlightState&) = delete;
    TextHighlightState& operator=(const TextHighlightState&) = delete;
    ~TextHighlightState();

    // Tracks whether the current text highlight has finished highlighting a
    // multi-click text selection, but has not yet exited text highlight state.
    // For example, the user may click text three times to select the line, but
    // may not have performed mouseup nor touchend. The user should still be in
    // text highlight state but should be unable to highlight any additional
    // text.
    bool finished_multi_click = false;

    // A mapping of 0-based page indices to a list of strokes on pages that
    // represent the user's highlighter text selections. Unlike drawing strokes
    // which are limited to one page, text selection may cover multiple pages.
    // For example, when the user has the highlighter brush selected, they may
    // select text from page A to page B. Strokes will be drawn to cover any
    // selected text and stored in the page index of the page they are on.
    std::map<int, std::vector<ink::Stroke>> highlight_strokes;

    // Details from the last input. Used to compensate for missed events, such
    // as a missed move event, or an end event that was consumed by a different
    // view and detected afterwards when PdfInkModule finally sees input events
    // again. Not wrapped in an `std::optional` because this state is only
    // active when the user is actively selecting text. The event time is
    // unused.
    EventDetails input_last_event;

    // Whether the text highlight was initiated by a keyboard event.
    bool initiated_by_keyboard = false;
  };

  // Drawing brush state changes that are pending the completion of an
  // in-progress stroke.
  struct PendingDrawingBrushState {
    SkColor color;
    float size;
    PdfInkBrush::Type type;
  };

  // Data used to draw a text highlight stroke. If `first_point` equals
  // `second_point`, then `second_point` is not used.
  // `brush_size` should cover the stroke on the smaller dimension of the
  // highlight rect.
  // All values are based on canonical coordinates.
  struct TextSelectionHighlightStrokeData {
    gfx::PointF first_point;
    gfx::PointF second_point;
    float brush_size;
  };

  // The transform to and clip page rect needed to render strokes on screen.
  struct TransformAndClipRect {
    ink::AffineTransform transform;
    SkRect clip_rect;
  };

  // Event handlers. Returns whether the event was handled or not.
  bool OnKeyDown(const blink::WebKeyboardEvent& event);
  bool OnMouseDown(const blink::WebMouseEvent& event);
  bool OnMouseUp(const blink::WebMouseEvent& event);
  bool OnMouseMove(const blink::WebMouseEvent& event);
  bool OnTouchStart(const blink::WebTouchEvent& event);
  bool OnTouchEnd(const blink::WebTouchEvent& event);
  bool OnTouchMove(const blink::WebTouchEvent& event);

  // Helper for event handlers above that deals with potentially missing events.
  // Can only be called when is_drawing_stroke() returns true.
  void MaybeFinishStrokeForMissingMouseUpEvent();

  // Return values have the same semantics as On{Mouse,Touch}*() above.
  bool StartStroke(const gfx::PointF& position,
                   base::TimeTicks timestamp,
                   ink::StrokeInput::ToolType tool_type,
                   const blink::WebPointerProperties* properties);
  bool ContinueStroke(const gfx::PointF& position,
                      base::TimeTicks timestamp,
                      ink::StrokeInput::ToolType tool_type,
                      const blink::WebPointerProperties* properties);
  bool FinishStroke(const gfx::PointF& position,
                    base::TimeTicks timestamp,
                    ink::StrokeInput::ToolType tool_type,
                    const blink::WebPointerProperties* properties);

  // Return values have the same semantics as On{Mouse,Touch}*() above.
  bool StartEraseStroke(const gfx::PointF& position,
                        ink::StrokeInput::ToolType tool_type);
  bool ContinueEraseStroke(const gfx::PointF& position,
                           ink::StrokeInput::ToolType tool_type);
  bool FinishEraseStroke(const gfx::PointF& position,
                         ink::StrokeInput::ToolType tool_type);

  // Shared code for the Erase methods above.
  void EraseHelper(const gfx::PointF& position, int page_index);

  // Return values have the same semantics as On{Mouse,Touch}*() above.
  bool StartTextHighlight(const gfx::PointF& position,
                          int click_count,
                          ink::StrokeInput::ToolType tool_type);
  bool ContinueTextHighlight(const gfx::PointF& position);
  bool FinishTextHighlight(const gfx::PointF& position,
                           bool is_multi_click,
                           ink::StrokeInput::ToolType tool_type);

  // Returns a highlighter stroke that matches the position and size of
  // `selection_rect`. `selection_rect` is in canonical coordinates.
  // On failure, return std::nullopt.
  std::optional<ink::Stroke> GetHighlightStrokeFromSelectionRect(
      const gfx::RectF& selection_rect);

  // Returns the data needed to create a text highlight stroke that covers
  // `selection_rect`. `selection_rect` is in canonical coordinates.
  TextSelectionHighlightStrokeData GetTextSelectionHighlightStrokeData(
      const gfx::RectF& selection_rect);

  // Converts PdfInkModuleClient's text selection to strokes and returns a
  // mapping of 0-based page indices to a list of those strokes. See comments
  // for `TextHighlightState::highlight_strokes`.
  std::map<int, std::vector<ink::Stroke>> GetTextSelectionAsStrokes();

  // Starts a timer for text selection multi-clicks that, when fired, will
  // report text highlight metrics.
  void StartTextSelectionMultiClickTimer(ink::StrokeInput::ToolType tool_type);

  // Stops the timer from `StartTextSelectionMultiClickTimer()` without
  // reporting any metrics.
  void StopTextSelectionMultiClickTimer();

  // Sets `using_stylus_instead_of_touch_` to true if `tool_type` is
  // `ink::StrokeInput::ToolType::kStylus`. Otherwise do nothing.
  void MaybeRecordPenInput(ink::StrokeInput::ToolType tool_type);

  // Returns true if `using_stylus_instead_of_touch_` is set, and `tool_type` is
  // `ink::StrokeInput::ToolType::kTouch`.
  bool ShouldIgnoreTouchInput(ink::StrokeInput::ToolType tool_type);

  void HandleAnnotationRedoMessage(const base::Value::Dict& message);
  void HandleAnnotationUndoMessage(const base::Value::Dict& message);
  void HandleFinishTextAnnotationMessage(const base::Value::Dict& message);
  void HandleGetAllTextAnnotationsMessage(const base::Value::Dict& message);
  void HandleGetAnnotationBrushMessage(const base::Value::Dict& message);
  void HandleSetAnnotationBrushMessage(const base::Value::Dict& message);
  void HandleSetAnnotationModeMessage(const base::Value::Dict& message);
  void HandleStartTextAnnotationMessage(const base::Value::Dict& message);

  bool is_drawing_stroke() const {
    return std::holds_alternative<DrawingStrokeState>(current_tool_state_);
  }
  bool is_erasing_stroke() const {
    return std::holds_alternative<EraserState>(current_tool_state_);
  }
  bool is_text_highlighting() const {
    return std::holds_alternative<TextHighlightState>(current_tool_state_);
  }
  const DrawingStrokeState& drawing_stroke_state() const {
    return std::get<DrawingStrokeState>(current_tool_state_);
  }
  DrawingStrokeState& drawing_stroke_state() {
    return std::get<DrawingStrokeState>(current_tool_state_);
  }
  const EraserState& erasing_stroke_state() const {
    return std::get<EraserState>(current_tool_state_);
  }
  EraserState& erasing_stroke_state() {
    return std::get<EraserState>(current_tool_state_);
  }
  const TextHighlightState& text_highlight_state() const {
    return std::get<TextHighlightState>(current_tool_state_);
  }
  TextHighlightState& text_highlight_state() {
    return std::get<TextHighlightState>(current_tool_state_);
  }

  // Returns true when the user is using a highlighter over selectable text at
  // `position`. Can only be called when is_drawing_stroke() returns true.
  //
  // - Only returns true when the text highlighting feature is enabled.
  bool IsHighlightingTextAtPosition(const gfx::PointF& position) const;

  // Returns the current brush. Must be in a drawing stroke state.
  PdfInkBrush& GetDrawingBrush();
  const PdfInkBrush& GetDrawingBrush() const;

  // Returns the brush with type `brush_type`.
  const PdfInkBrush& GetBrush(PdfInkBrush::Type brush_type) const;

  // Converts `current_tool_state_` into segments of `ink::InProgressStroke`.
  // Requires `current_tool_state_` to hold a `DrawingStrokeState`. If there is
  // no `DrawingStrokeState`, or the state currently has no inputs, then the
  // segments will be empty.
  std::vector<ink::InProgressStroke> CreateInProgressStrokeSegmentsFromInputs()
      const;

  // Wrapper around GetEventToCanonicalTransform(). `page_index` is the page
  // that the to-be-transformed position is on. The page must be visible.
  gfx::Transform GetEventToCanonicalTransformForPage(int page_index);
  // Inverse of GetEventToCanonicalTransformForPage(), for convenience.
  gfx::Transform GetCanonicalToEventTransformForPage(int page_index);

  // Helper to convert `position` to a canonical position and record it into
  // `current_tool_state_` for the indicated `timestamp`, `tool_type`, and
  // optional `properties`.
  // Can only be called when drawing. Returns whether the operation succeeded or
  // not.
  bool RecordStrokePosition(const gfx::PointF& position,
                            base::TimeTicks timestamp,
                            ink::StrokeInput::ToolType tool_type,
                            const blink::WebPointerProperties* properties);

  void ApplyUndoRedoCommands(const PdfInkUndoRedoModel::Commands& commands);
  void ApplyUndoRedoCommandsHelper(std::set<PdfInkUndoRedoModel::IdType> ids,
                                   bool should_draw);

  void ApplyUndoRedoDiscards(
      const PdfInkUndoRedoModel::DiscardedDrawCommands& discards);

  // Sets the cursor to a drawing/erasing brush cursor when necessary.
  void MaybeSetCursor();

  // Handles setting the cursor only for mousemove events at `position`. This
  // differs from `MaybeSetCursor()` in that it may also set the cursor to an
  // I-beam for text highlighting.
  void MaybeSetCursorOnMouseMove(const gfx::PointF& position);

  // Returns whether the drawing brush was set or not.
  bool MaybeSetDrawingBrush();

  void DrawStrokeInRenderer(ink::SkiaRenderer& skia_renderer,
                            SkCanvas& canvas,
                            int page_index,
                            const ink::Stroke& stroke);

  void DrawInProgressStrokeInRenderer(ink::SkiaRenderer& skia_renderer,
                                      SkCanvas& canvas,
                                      int page_index,
                                      const ink::InProgressStroke& stroke);

  // Returns the transform and the clip page rect needed to render strokes on
  // page `page_index`.
  TransformAndClipRect GetTransformAndClipRect(int page_index);

  // Helper that calls GenerateAndSendInkThumbnail() without needing to specify
  // the thumbnail size. This helper determines the size by asking
  // PdfInkModuleClient.
  void GenerateAndSendInkThumbnailInternal(int page_index);

  // Draws `strokes_` for `page_index` into `canvas`. Here, `canvas` only covers
  // the region for the page at `page_index`, so this only draws strokes for
  // that page, regardless of page visibility.
  bool DrawThumbnail(SkCanvas& canvas, int page_index);

  // Updates the page indices in `ink_updates` using
  // GenerateAndSendInkThumbnailInternal(), and updates the page indices in
  // `pdf_updates` using PdfInkModuleClient::RequestThumbnail().
  void RequestThumbnailUpdates(const base::flat_set<int>& ink_updates,
                               const base::flat_set<int>& pdf_updates);

  // Handles the callback for PDF thumbnail generation requests. Sends
  // `thumbnail` to the WebUI.
  void OnGotThumbnail(int page_index, Thumbnail thumbnail);

  const raw_ref<PdfInkModuleClient> client_;

  InkAnnotationMode mode_ = InkAnnotationMode::kOff;

  bool using_stylus_instead_of_touch_ = false;

  bool loaded_data_from_pdf_ = false;

  // Shapes loaded from the PDF.
  DocumentV2InkPathShapesMap loaded_v2_shapes_;

  // Generates IDs for use in FinishedStrokeState and PdfInkUndoRedoModel.
  StrokeIdGenerator stroke_id_generator_;

  // Store a PdfInkBrush for each brush type so that the brush parameters are
  // saved when swapping between brushes.  The PdfInkBrushes should not be
  // modified in the middle of an in-progress stroke.
  PdfInkBrush highlighter_brush_;
  PdfInkBrush pen_brush_;

  // The parameters that are to be applied to the drawing brushes when a new
  // stroke is started.  These can be modified at any time, including in the
  // middle of an in-progress stroke.
  std::optional<PendingDrawingBrushState> pending_drawing_brush_state_;

  // The state of the current tool that is in use.
  std::variant<DrawingStrokeState, EraserState, TextHighlightState>
      current_tool_state_;

  // The state of the strokes that have been completed.
  DocumentStrokesMap strokes_;

  PdfInkUndoRedoModel undo_redo_model_;

  // A timer used for reporting metrics during multi-click text selection.
  base::OneShotTimer text_selection_click_timer_;

  base::WeakPtrFactory<PdfInkModule> weak_factory_{this};
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_MODULE_H_
