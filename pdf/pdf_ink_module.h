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

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/values.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_ink_undo_redo_model.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/ink/src/ink/strokes/in_progress_stroke.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "ui/gfx/geometry/point_f.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

class SkCanvas;

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}  // namespace blink

namespace chrome_pdf {

class PdfInkBrush;
class PdfInkModuleClient;

class PdfInkModule {
 private:
  // Some initial definitions needed for internal working of public classes.

  // A stroke that has been completed, its ID, and whether it should be drawn
  // or not.
  struct FinishedStrokeState {
    FinishedStrokeState(ink::Stroke stroke, size_t id);
    FinishedStrokeState(const FinishedStrokeState&) = delete;
    FinishedStrokeState& operator=(const FinishedStrokeState&) = delete;
    FinishedStrokeState(FinishedStrokeState&&) noexcept;
    FinishedStrokeState& operator=(FinishedStrokeState&&) noexcept;
    ~FinishedStrokeState();

    // Coordinates for each stroke are stored in a canonical format specified in
    // pdf_ink_transform.h.
    ink::Stroke stroke;

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

 public:
  using StrokeInputPoints = std::vector<gfx::PointF>;

  // Each page of a document can have many strokes.  The input points for each
  // stroke are restricted to just one page.
  using PageStrokeInputPoints = std::vector<StrokeInputPoints>;

  // Mapping of a 0-based page index to the input points that make up the
  // strokes for that page.
  using DocumentStrokeInputPointsMap = std::map<int, PageStrokeInputPoints>;

  struct PageInkStroke {
    int page_index;
    raw_ref<const ink::Stroke> stroke;
  };

  // Iterator to get visible strokes.  Once created, the caller should ensure
  // that there is no further PdfInkModule interactions until the iterator has
  // been destroyed.
  class PageInkStrokeIterator {
   public:
    explicit PageInkStrokeIterator(const DocumentStrokesMap& strokes);
    PageInkStrokeIterator(const PageInkStrokeIterator&) = delete;
    PageInkStrokeIterator& operator=(const PageInkStrokeIterator&) = delete;
    ~PageInkStrokeIterator();

    // Gets the next visible stroke if there is one, and advances the internal
    // iterator to the next visible stroke.
    std::optional<PageInkStroke> GetNextStrokeAndAdvance();

   private:
    // Helper to advance to the next page which has visible strokes.  If there
    // is another page with visible strokes, performs the iterators
    // initialization to be able to get the visible strokes for it.  Leaves
    // `pages_iterator_` at end position if there are no more pages with
    // visible strokes.
    void AdvanceToNextPageWithVisibleStrokes();

    // Helper to advance to the next visible stroke for the current page, if
    // there is one.  Leaves `page_strokes_iterator_` at end position if there
    // are no more visible strokes.
    void AdvanceForCurrentPage();

    const raw_ref<const DocumentStrokesMap> strokes_;

    // Iterator for getting pages with visible strokes.
    DocumentStrokesMap::const_iterator pages_iterator_;

    // Iterator for getting visible strokes of a particular page.
    PageStrokes::const_iterator page_strokes_iterator_;
  };

  explicit PdfInkModule(PdfInkModuleClient& client);
  PdfInkModule(const PdfInkModule&) = delete;
  PdfInkModule& operator=(const PdfInkModule&) = delete;
  ~PdfInkModule();

  bool enabled() const { return enabled_; }

  // Draws `strokes_` and `inputs_` into `canvas`. Here, `canvas` covers the
  // visible content area, so this only draws strokes for visible pages.
  void Draw(SkCanvas& canvas);

  // Draws `strokes_` for `page_index` into `canvas`. Here, `canvas` only covers
  // the region for the page at `page_index`, so this only draws strokes for
  // that page, regardless of page visibility.
  bool DrawThumbnail(SkCanvas& canvas, int page_index);

  // Gets an iterator for the visible strokes across all pages.
  // Modifying the set of visible strokes while using the iterator is not
  // supported and can result in undefined behavior.
  PageInkStrokeIterator GetVisibleStrokesIterator();

  // Returns whether the event was handled or not.
  bool HandleInputEvent(const blink::WebInputEvent& event);

  // Returns whether the message was handled or not.
  bool OnMessage(const base::Value::Dict& message);

  // Informs PdfInkModule that the plugin geometry changed.
  void OnGeometryChanged();

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

 private:
  struct DrawingStrokeState {
    DrawingStrokeState();
    DrawingStrokeState(const DrawingStrokeState&) = delete;
    DrawingStrokeState& operator=(const DrawingStrokeState&) = delete;
    ~DrawingStrokeState();

    // The current brush to use for drawing strokes. Never null.
    std::unique_ptr<PdfInkBrush> brush;

    std::optional<base::TimeTicks> start_time;

    // The 0-based page index which is currently being stroked.
    int page_index = -1;

    // The event position for the last input.  Coordinates match the
    // screen-based position that are provided during stroking from
    // `blink::WebMouseEvent` positions.  Used after stroking has already
    // started, for invalidation and for extrapolating where a stroke crosses
    // the page boundary.
    std::optional<gfx::PointF> input_last_event_position;

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
    size_t GetIdAndAdvance();

    void ResetIdTo(size_t id);

   private:
    // The next available ID for use in FinishedStrokeState.
    size_t next_stroke_id_ = 0;
  };

  struct EraserState {
    EraserState();
    EraserState(const EraserState&) = delete;
    EraserState& operator=(const EraserState&) = delete;
    ~EraserState();

    bool erasing = false;
    base::flat_set<int> page_indices_with_erased_strokes;
    float eraser_size = 0;
  };

  // Returns whether the event was handled or not.
  bool OnMouseDown(const blink::WebMouseEvent& event);
  bool OnMouseUp(const blink::WebMouseEvent& event);
  bool OnMouseMove(const blink::WebMouseEvent& event);

  // Return values have the same semantics as OnMouse()* above.
  bool StartStroke(const gfx::PointF& position, base::TimeTicks timestamp);
  bool ContinueStroke(const gfx::PointF& position, base::TimeTicks timestamp);
  bool FinishStroke(const gfx::PointF& position, base::TimeTicks timestamp);

  // Return values have the same semantics as OnMouse*() above.
  bool StartEraseStroke(const gfx::PointF& position);
  bool ContinueEraseStroke(const gfx::PointF& position);
  bool FinishEraseStroke(const gfx::PointF& position);

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

  // Converts `current_tool_state_` into segments of `ink::InProgressStroke`.
  // Requires `current_tool_state_` to hold a `DrawingStrokeState`. If there is
  // no `DrawingStrokeState`, or the state currently has no inputs, then the
  // segments will be empty.
  std::vector<ink::InProgressStroke> CreateInProgressStrokeSegmentsFromInputs()
      const;

  // Wrapper around EventPositionToCanonicalPosition(). `page_index` is the page
  // that `position` is on. The page must be visible.
  gfx::PointF ConvertEventPositionToCanonicalPosition(
      const gfx::PointF& position,
      int page_index);

  // Helper to convert `position` to a canonical position and record it into
  // `current_tool_state_` for the indicated time. Can only be called when
  // drawing.
  void RecordStrokePosition(const gfx::PointF& position,
                            base::TimeTicks timestamp);

  void ApplyUndoRedoCommands(const PdfInkUndoRedoModel::Commands& commands);
  void ApplyUndoRedoCommandsHelper(std::set<size_t> ids, bool should_draw);

  void ApplyUndoRedoDiscards(
      const PdfInkUndoRedoModel::DiscardedDrawCommands& discards);

  void MaybeSetCursor();

  const raw_ref<PdfInkModuleClient> client_;

  bool enabled_ = false;

  // Generates IDs for use in FinishedStrokeState and PdfInkUndoRedoModel.
  StrokeIdGenerator stroke_id_generator_;

  // The state of the current tool that is in use.
  absl::variant<DrawingStrokeState, EraserState> current_tool_state_;

  // The state of the strokes that have been completed.
  DocumentStrokesMap strokes_;

  PdfInkUndoRedoModel undo_redo_model_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_MODULE_H_
