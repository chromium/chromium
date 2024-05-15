// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include <algorithm>

#include "base/numerics/clamped_math.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

namespace {

bool ShouldUpdateTextInputState(const ui::mojom::TextInputState& old_state,
                                const ui::mojom::TextInputState& new_state) {
#if defined(USE_AURA)
  return old_state.node_id != new_state.node_id ||
         old_state.type != new_state.type || old_state.mode != new_state.mode ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#elif BUILDFLAG(IS_APPLE)
  return old_state.type != new_state.type ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#elif BUILDFLAG(IS_ANDROID)
  // On Android, TextInputState update is sent only if there is some change in
  // the state. So the new state is always different.
  return true;
#else
  NOTREACHED_IN_MIGRATION();
  return true;
#endif
}

}  // namespace

TextInputManager::TextInputManager() : active_view_(nullptr) {}

TextInputManager::~TextInputManager() {
  // If there is an active view, we should unregister it first so that the
  // the tab's top-level RWHV will be notified about |TextInputState.type|
  // resetting to none (i.e., we do not have an active RWHV anymore).
  if (active_view_)
    Unregister(active_view_);

  // Unregister all the remaining views.
  std::vector<RenderWidgetHostViewBase*> views;
  for (auto const& pair : text_input_state_map_)
    views.push_back(pair.first);

  for (auto* view : views)
    Unregister(view);
}

RenderWidgetHostImpl* TextInputManager::GetActiveWidget() const {
  return !!active_view_ ? static_cast<RenderWidgetHostImpl*>(
                              active_view_->GetRenderWidgetHost())
                        : nullptr;
}

const ui::mojom::TextInputState* TextInputManager::GetTextInputState() const {
  if (!active_view_) {
    return nullptr;
  }

  return text_input_state_map_.at(active_view_).get();
}

gfx::Range TextInputManager::GetAutocorrectRange() const {
  if (!active_view_)
    return gfx::Range();

  for (auto const& pair : text_input_state_map_) {
    for (const auto& ime_text_span_info : pair.second->ime_text_spans_info) {
      if (ime_text_span_info->span.type ==
          ui::ImeTextSpan::Type::kAutocorrect) {
        return gfx::Range(ime_text_span_info->span.start_offset,
                          ime_text_span_info->span.end_offset);
      }
    }
  }
  return gfx::Range();
}

std::optional<ui::GrammarFragment> TextInputManager::GetGrammarFragment(
    gfx::Range range) const {
  if (!active_view_)
    return std::nullopt;

  for (const auto& ime_text_span_info :
       text_input_state_map_.at(active_view_)->ime_text_spans_info) {
    if (ime_text_span_info->span.type ==
            ui::ImeTextSpan::Type::kGrammarSuggestion &&
        ime_text_span_info->span.suggestions.size() > 0) {
      auto span_range = gfx::Range(ime_text_span_info->span.start_offset,
                                   ime_text_span_info->span.end_offset);
      if (span_range.Contains(range)) {
        return ui::GrammarFragment(span_range,
                                   ime_text_span_info->span.suggestions[0]);
      }
    }
  }
  return std::nullopt;
}

const TextInputManager::SelectionRegion* TextInputManager::GetSelectionRegion(
    RenderWidgetHostViewBase* view) const {
  DCHECK(!view || IsRegistered(view));
  if (!view)
    view = active_view_;
  return view ? &selection_region_map_.at(view) : nullptr;
}

const TextInputManager::CompositionRangeInfo*
TextInputManager::GetCompositionRangeInfo() const {
  return active_view_ ? &composition_range_info_map_.at(active_view_) : nullptr;
}

const TextInputManager::TextSelection* TextInputManager::GetTextSelection(
    RenderWidgetHostViewBase* view) const {
  DCHECK(!view || IsRegistered(view));
  if (!view)
    view = active_view_;
  // A crash occurs when we end up here with an unregistered view.
  // See crbug.com/735980
  // TODO(ekaramad): Take a deeper look why this is happening.
  return (view && IsRegistered(view)) ? &text_selection_map_.at(view) : nullptr;
}

const std::optional<gfx::Rect> TextInputManager::GetTextControlBounds() const {
  const ui::mojom::TextInputState* state = GetTextInputState();
  if (!active_view_ || !state || !state->edit_context_control_bounds)
    return std::nullopt;

  auto control_bounds = state->edit_context_control_bounds.value();
  auto new_top_left =
      active_view_->TransformPointToRootCoordSpace(control_bounds.origin());
  return std::optional<gfx::Rect>(
      gfx::Rect(new_top_left, control_bounds.size()));
}

const std::optional<gfx::Rect> TextInputManager::GetTextSelectionBounds()
    const {
  const ui::mojom::TextInputState* state = GetTextInputState();
  if (!active_view_ || !state || !state->edit_context_selection_bounds)
    return std::nullopt;

  auto selection_bounds = state->edit_context_selection_bounds.value();
  auto new_top_left =
      active_view_->TransformPointToRootCoordSpace(selection_bounds.origin());
  return std::optional<gfx::Rect>(
      gfx::Rect(new_top_left, selection_bounds.size()));
}

void TextInputManager::UpdateTextInputState(
    RenderWidgetHostViewBase* view,
    const ui::mojom::TextInputState& text_input_state) {
  DCHECK(IsRegistered(view));

  if (text_input_state.type == ui::TEXT_INPUT_TYPE_NONE &&
      active_view_ != view) {
    // We reached here because an IPC is received to reset the TextInputState
    // for |view|. But |view| != |active_view_|, which suggests that at least
    // one other view has become active and we have received the corresponding
    // IPC from their RenderWidget sooner than this one. That also means we have
    // already synthesized the loss of TextInputState for the |view| before (see
    // below). So we can forget about this method ever being called (no observer
    // calls necessary).
    // NOTE: Android requires state to be returned even when the current state
    // is/becomes NONE. Otherwise IME may become irresponsive.
#if !BUILDFLAG(IS_ANDROID)
    return;
#endif
  }

  // Since |view| is registered, we already have a previous value for its
  // TextInputState.
  bool changed = ShouldUpdateTextInputState(*text_input_state_map_[view],
                                            text_input_state);
  TRACE_EVENT2(
      "ime", "TextInputManager::UpdateTextInputState", "changed", changed,
      "text_input_state - type, selection, composition, "
      "show_ime_if_needed, control_bounds",
      base::NumberToString(text_input_state.type) + ", " +
          text_input_state.selection.ToString() + ", " +
          (text_input_state.composition.has_value()
               ? text_input_state.composition->ToString()
               : "") +
          ", " + base::NumberToString(text_input_state.show_ime_if_needed) +
          ", " +
          (text_input_state.edit_context_control_bounds.has_value()
               ? text_input_state.edit_context_control_bounds->ToString()
               : ""));
  text_input_state_map_[view] = text_input_state.Clone();
  for (const auto& ime_text_span_info :
       text_input_state_map_[view]->ime_text_spans_info) {
    const gfx::Rect& bounds = ime_text_span_info->bounds;
    ime_text_span_info->bounds = gfx::Rect(
        view->TransformPointToRootCoordSpace(bounds.origin()), bounds.size());
  }

  // If |view| is different from |active_view| and its |TextInputState.type| is
  // not NONE, |active_view_| should change to |view|.
  if (text_input_state.type != ui::TEXT_INPUT_TYPE_NONE &&
      active_view_ != view) {
    if (active_view_) {
      // Ideally, we should always receive an IPC from |active_view_|'s
      // RenderWidget to reset its |TextInputState.type| to NONE, before any
      // other RenderWidget updates its TextInputState. But there is no
      // guarantee in the order of IPCs from different RenderWidgets and another
      // RenderWidget's IPC might arrive sooner and we reach here. To make the
      // IME behavior identical to the non-OOPIF case, we have to manually reset
      // the state for |active_view_|.
      text_input_state_map_[active_view_]->type = ui::TEXT_INPUT_TYPE_NONE;
      RenderWidgetHostViewBase* active_view = active_view_;
      active_view_ = nullptr;
      NotifyObserversAboutInputStateUpdate(active_view, true);
    }
    active_view_ = view;
  }

  // If the state for |active_view_| is none, then we no longer have an
  // |active_view_|.
  if (active_view_ == view && text_input_state.type == ui::TEXT_INPUT_TYPE_NONE)
    active_view_ = nullptr;

  NotifyObserversAboutInputStateUpdate(view, changed);
}

void TextInputManager::ImeCancelComposition(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));
  for (auto& observer : observer_list_)
    observer.OnImeCancelComposition(this, view);
}

void TextInputManager::SelectionBoundsChanged(
    RenderWidgetHostViewBase* view,
    const gfx::Rect& anchor_rect,
    base::i18n::TextDirection anchor_dir,
    const gfx::Rect& focus_rect,
    base::i18n::TextDirection focus_dir,
    const gfx::Rect& bounding_box,
    bool is_anchor_first) {
  DCHECK(IsRegistered(view));
  // Converting the anchor point to root's coordinate space (for child frame
  // views).
  gfx::Point anchor_origin_transformed =
      view->TransformPointToRootCoordSpace(anchor_rect.origin());

  gfx::SelectionBound anchor_bound, focus_bound;

  anchor_bound.SetEdge(gfx::PointF(anchor_origin_transformed),
                       gfx::PointF(view->TransformPointToRootCoordSpace(
                           anchor_rect.bottom_left())));
  focus_bound.SetEdge(
      gfx::PointF(view->TransformPointToRootCoordSpace(focus_rect.origin())),
      gfx::PointF(
          view->TransformPointToRootCoordSpace(focus_rect.bottom_left())));

  if (anchor_rect == focus_rect) {
    anchor_bound.set_type(gfx::SelectionBound::CENTER);
    focus_bound.set_type(gfx::SelectionBound::CENTER);
  } else {
    // Whether text is LTR at the anchor handle.
    bool anchor_LTR = anchor_dir == base::i18n::LEFT_TO_RIGHT;
    // Whether text is LTR at the focus handle.
    bool focus_LTR = focus_dir == base::i18n::LEFT_TO_RIGHT;

    if ((is_anchor_first && anchor_LTR) || (!is_anchor_first && !anchor_LTR)) {
      anchor_bound.set_type(gfx::SelectionBound::LEFT);
    } else {
      anchor_bound.set_type(gfx::SelectionBound::RIGHT);
    }
    if ((is_anchor_first && focus_LTR) || (!is_anchor_first && !focus_LTR)) {
      focus_bound.set_type(gfx::SelectionBound::RIGHT);
    } else {
      focus_bound.set_type(gfx::SelectionBound::LEFT);
    }
  }

  // Transform `bounding_box` to the top-level frame's coordinate space.
  std::vector<gfx::Point> bounding_box_vertice = {
      bounding_box.origin(), bounding_box.top_right(),
      bounding_box.bottom_left(), bounding_box.bottom_right()};
  std::vector<int> x_after_transform;
  std::vector<int> y_after_transform;
  for (const auto& vertex : bounding_box_vertice) {
    const gfx::Point vertex_after_transform =
        view->TransformPointToRootCoordSpace(vertex);
    x_after_transform.push_back(vertex_after_transform.x());
    y_after_transform.push_back(vertex_after_transform.y());
  }

  std::sort(x_after_transform.begin(), x_after_transform.end());
  std::sort(y_after_transform.begin(), y_after_transform.end());

  const gfx::Point bounding_box_origin_after_transform(x_after_transform[0],
                                                       y_after_transform[0]);
  const gfx::Point bounding_box_bottom_right_after_transform(
      x_after_transform.back(), y_after_transform.back());
  const gfx::Rect bounding_box_transformed(
      bounding_box_origin_after_transform,
      gfx::Size(base::ClampSub(bounding_box_bottom_right_after_transform.x(),
                               bounding_box_origin_after_transform.x()),
                base::ClampSub(bounding_box_bottom_right_after_transform.y(),
                               bounding_box_origin_after_transform.y())));

  if (anchor_bound == selection_region_map_[view].anchor &&
      focus_bound == selection_region_map_[view].focus &&
      bounding_box_transformed == selection_region_map_[view].bounding_box) {
    return;
  }

  selection_region_map_[view].anchor = anchor_bound;
  selection_region_map_[view].focus = focus_bound;
  selection_region_map_[view].bounding_box = bounding_box_transformed;

  if (anchor_rect == focus_rect) {
    selection_region_map_[view].caret_rect.set_origin(
        anchor_origin_transformed);
    selection_region_map_[view].caret_rect.set_size(anchor_rect.size());
  }
  selection_region_map_[view].first_selection_rect.set_origin(
      anchor_origin_transformed);
  selection_region_map_[view].first_selection_rect.set_size(anchor_rect.size());

  NotifySelectionBoundsChanged(view);
}

void TextInputManager::NotifySelectionBoundsChanged(
    RenderWidgetHostViewBase* view) {
  for (auto& observer : observer_list_)
    observer.OnSelectionBoundsChanged(this, view);
}

// TODO(ekaramad): We use |range| only on Mac OS; but we still track its value
// here for other platforms. See if there is a nice way around this with minimal
// #ifdefs for platform specific code (https://crbug.com/602427).
// This also applies to |line_bounds| which are only used on Android.
void TextInputManager::ImeCompositionRangeChanged(
    RenderWidgetHostViewBase* view,
    const gfx::Range& range,
    const std::optional<std::vector<gfx::Rect>>& character_bounds,
    const std::optional<std::vector<gfx::Rect>>& line_bounds) {
  DCHECK(IsRegistered(view));

  if (character_bounds.has_value()) {
    composition_range_info_map_[view].character_bounds.clear();

    // The values for the bounds should be converted to root view's coordinates
    // before being stored.
    for (auto& rect : character_bounds.value()) {
      composition_range_info_map_[view].character_bounds.emplace_back(
          view->TransformPointToRootCoordSpace(rect.origin()), rect.size());
    }

    composition_range_info_map_[view].range.set_start(range.start());
    composition_range_info_map_[view].range.set_end(range.end());
  }
  // Transform the values in line bounds to the root coordinate space if they
  // exist.
  std::optional<std::vector<gfx::Rect>> transformed_line_bounds;
  if (line_bounds.has_value()) {
    transformed_line_bounds.emplace();
    for (auto& rect : line_bounds.value()) {
      transformed_line_bounds->emplace_back(
          view->TransformPointToRootCoordSpace(rect.origin()), rect.size());
    }
  }

  for (auto& observer : observer_list_) {
    observer.OnImeCompositionRangeChanged(
        this, view, character_bounds.has_value(), transformed_line_bounds);
  }
}

void TextInputManager::SelectionChanged(RenderWidgetHostViewBase* view,
                                        const std::u16string& text,
                                        size_t offset,
                                        const gfx::Range& range) {
  DCHECK(IsRegistered(view));
  text_selection_map_[view].SetSelection(text, offset, range);
  for (auto& observer : observer_list_)
    observer.OnTextSelectionChanged(this, view);
}

void TextInputManager::Register(RenderWidgetHostViewBase* view) {
  DCHECK(!IsRegistered(view));
  text_input_state_map_[view] = ui::mojom::TextInputState::New();
  selection_region_map_[view] = SelectionRegion();
  composition_range_info_map_[view] = CompositionRangeInfo();
  text_selection_map_[view] = TextSelection();
}

void TextInputManager::Unregister(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));

  text_input_state_map_.erase(view);
  selection_region_map_.erase(view);
  composition_range_info_map_.erase(view);
  text_selection_map_.erase(view);

  if (active_view_ == view) {
    active_view_ = nullptr;
    NotifyObserversAboutInputStateUpdate(view, true);
  }
  view->DidUnregisterFromTextInputManager(this);
}

bool TextInputManager::IsRegistered(RenderWidgetHostViewBase* view) const {
  return text_input_state_map_.count(view) == 1;
}

void TextInputManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TextInputManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool TextInputManager::HasObserver(Observer* observer) const {
  return observer_list_.HasObserver(observer);
}

size_t TextInputManager::GetRegisteredViewsCountForTesting() {
  return text_input_state_map_.size();
}

ui::TextInputType TextInputManager::GetTextInputTypeForViewForTesting(
    RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));
  return text_input_state_map_[view]->type;
}

const gfx::Range* TextInputManager::GetCompositionRangeForTesting() const {
  if (auto* info = GetCompositionRangeInfo())
    return &info->range;
  return nullptr;
}

void TextInputManager::NotifyObserversAboutInputStateUpdate(
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  for (auto& observer : observer_list_)
    observer.OnUpdateTextInputStateCalled(this, updated_view, did_update_state);
}

TextInputManager::SelectionRegion::SelectionRegion() = default;

TextInputManager::SelectionRegion::SelectionRegion(
    const SelectionRegion& other) = default;

TextInputManager::SelectionRegion& TextInputManager::SelectionRegion::operator=(
    const SelectionRegion& other) = default;

TextInputManager::CompositionRangeInfo::CompositionRangeInfo() = default;

TextInputManager::CompositionRangeInfo::CompositionRangeInfo(
    const CompositionRangeInfo& other) = default;

TextInputManager::CompositionRangeInfo::~CompositionRangeInfo() = default;

TextInputManager::TextSelection::TextSelection()
    : offset_(0), range_(gfx::Range::InvalidRange()) {}

TextInputManager::TextSelection::TextSelection(const TextSelection& other) =
    default;

TextInputManager::TextSelection::~TextSelection() = default;

void TextInputManager::TextSelection::SetSelection(const std::u16string& text,
                                                   size_t offset,
                                                   const gfx::Range& range) {
  text_ = text;
  range_.set_start(range.start());
  range_.set_end(range.end());
  offset_ = offset;

  // Update the selected text.
  selected_text_.clear();
  if (!text.empty() && !range.is_empty()) {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();
    if (pos + n > text.length()) {
      LOG(WARNING)
          << "The text cannot fully cover range (selection's end point "
             "exceeds text length).";
    }

    if (pos >= text.length()) {
      LOG(WARNING) << "The text cannot cover range (selection range's starting "
                      "point exceeds text length).";
    } else {
      selected_text_.append(text.substr(pos, n));
    }
  }
}

}  // namespace content
