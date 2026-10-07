// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

namespace {

#if defined(USE_AURA)
bool ShouldUpdateForFlagChanges(int old_flags, int new_flags) {
#if BUILDFLAG(IS_WIN)
  if ((old_flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD) &&
      (new_flags & ui::TEXT_INPUT_FLAG_HAS_BEEN_PASSWORD)) {
    // The custom password flag adds no new TSF semantics once a field has been
    // a native password field. Refocusing TSF for this flag alone changes the
    // virtual keyboard layout on older versions of Windows.
    old_flags &= ~ui::TEXT_INPUT_FLAG_HAS_BEEN_CUSTOM_PASSWORD;
    new_flags &= ~ui::TEXT_INPUT_FLAG_HAS_BEEN_CUSTOM_PASSWORD;
  }
#endif
  return old_flags != new_flags;
}
#endif  // defined(USE_AURA)

bool ShouldUpdateTextInputState(const ui::mojom::TextInputState& old_state,
                                const ui::mojom::TextInputState& new_state) {
#if defined(USE_AURA)
  return old_state.node_id != new_state.node_id ||
         old_state.type != new_state.type || old_state.mode != new_state.mode ||
         ShouldUpdateForFlagChanges(old_state.flags, new_state.flags) ||
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
  NOTREACHED();
#endif
}

// We want to validate with the viewport's rect. However this lookup can be
// invoked on a `RenderWidgetHostViewChildFrame` which has been disconnected
// from the viewport. In such a case we return the requested size of the child
// view.
gfx::Rect GetRootOrFallbackViewportRect(RenderWidgetHostViewBase* view) {
  auto* root_view = view->GetRootView();
  if (root_view) {
    return gfx::Rect(root_view->GetVisibleViewportSize());
  } else {
    return gfx::Rect(view->GetRequestedRendererSize());
  }
}

// Transforms `rect` to the root view's coordinate space and clamps it to the
// `viewport_rect`.
gfx::Rect TransformAndClampBounds(RenderWidgetHostViewBase* view,
                                  const gfx::Rect& rect,
                                  const gfx::Rect& viewport_rect) {
  gfx::Point origin_transformed =
      view->TransformPointToRootCoordSpace(rect.origin());
  gfx::Point bottom_left_transformed =
      view->TransformPointToRootCoordSpace(rect.bottom_left());
  gfx::Rect transformed_rect(
      origin_transformed, gfx::Size(rect.width(), bottom_left_transformed.y() -
                                                      origin_transformed.y()));
  transformed_rect.AdjustToFit(viewport_rect);
  return transformed_rect;
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
  while (!view_map_.empty()) {
    Unregister(view_map_.begin()->first);
  }
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

  return view_map_.at(active_view_).text_input_state.get();
}

gfx::Range TextInputManager::GetAutocorrectRange() const {
  if (!active_view_)
    return gfx::Range();

  for (const auto& ime_text_span_info :
       view_map_.at(active_view_).text_input_state->ime_text_spans_info) {
    if (ime_text_span_info->span.type == ui::ImeTextSpan::Type::kAutocorrect) {
      return gfx::Range(ime_text_span_info->span.start_offset,
                        ime_text_span_info->span.end_offset);
    }
  }
  return gfx::Range();
}

std::optional<ui::GrammarFragment> TextInputManager::GetGrammarFragment(
    gfx::Range range) const {
  if (!active_view_)
    return std::nullopt;

  for (const auto& ime_text_span_info :
       view_map_.at(active_view_).text_input_state->ime_text_spans_info) {
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
  // TODO(crbug.com/552952740): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(!view || IsRegistered(view));
  if (!view)
    view = active_view_;
  if (!view) {
    return nullptr;
  }
  const ViewState& view_state = view_map_.at(view);
  if (view_state.edit_context_selection_region) {
    return &*view_state.edit_context_selection_region;
  }
  return &view_state.selection_region;
}

const TextInputManager::CompositionRangeInfo*
TextInputManager::GetCompositionRangeInfo() const {
  return active_view_ ? &view_map_.at(active_view_).composition_range_info
                      : nullptr;
}

#if BUILDFLAG(IS_WIN)
const blink::mojom::ProximateCharacterRangeBounds*
TextInputManager::GetProximateCharacterBoundsInfo(
    const RenderWidgetHostViewBase& view) const {
  // TODO(crbug.com/355578906): Remove const_cast<RenderWidgetHostViewBase*>,
  // which is needed because TextInputManager::ViewMap has mutable
  // `RenderWidgetHostViewBase*` keys and the two RenderWidgetHostViewAura
  // callers are const methods passing (*this).
  // - RenderWidgetHostViewAura::GetProximateCharacterBounds
  // - RenderWidgetHostViewAura::GetProximateCharacterIndexFromPoint
  const auto found =
      view_map_.find(const_cast<RenderWidgetHostViewBase*>(&view));
  return found != view_map_.end()
             ? found->second.proximate_character_bounds.get()
             : nullptr;
}
#endif  // BUILDFLAG(IS_WIN)

const TextInputManager::TextSelection* TextInputManager::GetTextSelection(
    RenderWidgetHostViewBase* view) const {
  CHECK(!view || IsRegistered(view), base::NotFatalUntil::M153);
  if (!view)
    view = active_view_;
  // A crash occurs when we end up here with an unregistered view.
  // See crbug.com/735980
  // TODO(ekaramad): Take a deeper look why this is happening.
  if (!view) {
    return nullptr;
  }
  const auto found = view_map_.find(view);
  return found != view_map_.end() ? &found->second.text_selection : nullptr;
}

const std::optional<gfx::Rect> TextInputManager::GetTextControlBounds() const {
  const ui::mojom::TextInputState* state = GetTextInputState();
  if (!active_view_ || !state || !state->edit_context_control_bounds)
    return std::nullopt;

  auto control_bounds = state->edit_context_control_bounds.value();
  auto new_top_left =
      active_view_->TransformPointToRootCoordSpace(control_bounds.origin());
  control_bounds.set_origin(new_top_left);
  control_bounds.AdjustToFit(GetRootOrFallbackViewportRect(active_view_));
  return control_bounds;
}

const std::optional<gfx::Rect> TextInputManager::GetTextSelectionBounds()
    const {
  const ui::mojom::TextInputState* state = GetTextInputState();
  if (!active_view_ || !state || !state->edit_context_selection_bounds)
    return std::nullopt;

  auto selection_bounds = state->edit_context_selection_bounds.value();
  auto new_top_left =
      active_view_->TransformPointToRootCoordSpace(selection_bounds.origin());
  selection_bounds.set_origin(new_top_left);
  selection_bounds.AdjustToFit(GetRootOrFallbackViewportRect(active_view_));
  return selection_bounds;
}

void TextInputManager::UpdateTextInputState(
    RenderWidgetHostViewBase* view,
    const ui::mojom::TextInputState& text_input_state) {
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);

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
  ViewState& view_state = view_map_.at(view);
  bool changed = ShouldUpdateTextInputState(*view_state.text_input_state,
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
  view_state.text_input_state = text_input_state.Clone();
  const gfx::Rect viewport_rect = GetRootOrFallbackViewportRect(view);
  for (const auto& ime_text_span_info :
       view_state.text_input_state->ime_text_spans_info) {
    if (!ime_text_span_info->bounds.IsEmpty()) {
      ime_text_span_info->bounds = TransformAndClampBounds(
          view, ime_text_span_info->bounds, viewport_rect);
    }
  }

  std::optional<SelectionRegion> new_edit_context_region;
  if (text_input_state.edit_context_selection_bounds &&
      *text_input_state.edit_context_selection_bounds != gfx::Rect()) {
    gfx::Rect selection_bounds =
        *text_input_state.edit_context_selection_bounds;
    selection_bounds.set_origin(
        view->TransformPointToRootCoordSpace(selection_bounds.origin()));
    selection_bounds.AdjustToFit(viewport_rect);
    SelectionRegion& region = new_edit_context_region.emplace();
    region.anchor.SetEdge(gfx::PointF(selection_bounds.origin()),
                          gfx::PointF(selection_bounds.bottom_left()));
    region.anchor.set_type(gfx::SelectionBound::CENTER);
    region.focus.SetEdge(gfx::PointF(selection_bounds.top_right()),
                         gfx::PointF(selection_bounds.bottom_right()));
    region.focus.set_type(gfx::SelectionBound::CENTER);
    region.bounding_box = selection_bounds;
    region.caret_rect = selection_bounds;
    region.first_selection_rect = selection_bounds;
  }
  const bool edit_context_bounds_changed =
      new_edit_context_region != view_state.edit_context_selection_region;
  view_state.edit_context_selection_region = std::move(new_edit_context_region);

  // Observers can run nested message loops (e.g. a third-party IME pumping
  // messages inside InputMethod::OnTextInputTypeChanged()). Those can destroy
  // the WebContents, and with it |this|, or unregister |view|. Re-validate
  // both after every notification below before touching any state.
  base::WeakPtr<TextInputManager> weak_this = weak_ptr_factory_.GetWeakPtr();
  base::WeakPtr<RenderWidgetHostViewBase> weak_view = view->GetWeakPtr();

  // If |view| is different from |active_view| and its |TextInputState.type| is
  // not NONE, |active_view_| should change to |view| only if |view| has focus.
  if (text_input_state.type != ui::TEXT_INPUT_TYPE_NONE &&
      active_view_ != view && IsViewFocused(view)) {
    if (active_view_) {
      // Ideally, we should always receive an IPC from |active_view_|'s
      // RenderWidget to reset its |TextInputState.type| to NONE, before any
      // other RenderWidget updates its TextInputState. But there is no
      // guarantee in the order of IPCs from different RenderWidgets and
      // another RenderWidget's IPC might arrive sooner and we reach here. To
      // make the IME behavior identical to the non-OOPIF case, we have to
      // manually reset the state for |active_view_|.
      ViewState& active_view_state = view_map_.at(active_view_);
      active_view_state.text_input_state->type = ui::TEXT_INPUT_TYPE_NONE;
      active_view_state.edit_context_selection_region.reset();
      RenderWidgetHostViewBase* active_view = active_view_;
      active_view_ = nullptr;
      NotifyObserversAboutInputStateUpdate(active_view, true);
      // |view| may also have been unregistered without being destroyed (e.g.
      // moved to another TextInputManager); it must not become |active_view_|.
      if (!weak_this || !weak_view || !IsRegistered(view)) {
        return;
      }
    }
    active_view_ = view;
  }

  // If the state for |active_view_| is none, or if |active_view_| is no longer
  // focused, then we no longer have an |active_view_|.
  if (active_view_ == view &&
      (text_input_state.type == ui::TEXT_INPUT_TYPE_NONE ||
       !IsViewFocused(view))) {
    active_view_ = nullptr;
  }

  NotifyObserversAboutInputStateUpdate(view, changed);
  if (!weak_this || !weak_view) {
    return;
  }
  if (edit_context_bounds_changed && active_view_ == view) {
    NotifySelectionBoundsChanged(view);
  }
}

#if BUILDFLAG(IS_WIN)
void TextInputManager::UpdateProximateCharacterBounds(
    RenderWidgetHostViewBase& view,
    blink::mojom::ProximateCharacterRangeBoundsPtr proximate_bounds) {
  auto found = view_map_.find(&view);
  if (found != view_map_.end()) {
    found->second.proximate_character_bounds = std::move(proximate_bounds);
  }
}
#endif  // BUILDFLAG(IS_WIN)

void TextInputManager::ImeCancelComposition(RenderWidgetHostViewBase* view) {
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);
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
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);

  gfx::Rect viewport_rect = GetRootOrFallbackViewportRect(view);

  gfx::Rect transformed_anchor_rect =
      TransformAndClampBounds(view, anchor_rect, viewport_rect);
  gfx::Rect transformed_focus_rect =
      TransformAndClampBounds(view, focus_rect, viewport_rect);

  gfx::SelectionBound anchor_bound, focus_bound;

  anchor_bound.SetEdge(gfx::PointF(transformed_anchor_rect.origin()),
                       gfx::PointF(transformed_anchor_rect.bottom_left()));
  focus_bound.SetEdge(gfx::PointF(transformed_focus_rect.origin()),
                      gfx::PointF(transformed_focus_rect.bottom_left()));

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
  int min_x = std::numeric_limits<int>::max();
  int max_x = std::numeric_limits<int>::min();
  int min_y = std::numeric_limits<int>::max();
  int max_y = std::numeric_limits<int>::min();
  for (const gfx::Point& vertex :
       {bounding_box.origin(), bounding_box.top_right(),
        bounding_box.bottom_left(), bounding_box.bottom_right()}) {
    const gfx::Point vertex_after_transform =
        view->TransformPointToRootCoordSpace(vertex);
    min_x = std::min(min_x, vertex_after_transform.x());
    max_x = std::max(max_x, vertex_after_transform.x());
    min_y = std::min(min_y, vertex_after_transform.y());
    max_y = std::max(max_y, vertex_after_transform.y());
  }

  const gfx::Rect bounding_box_transformed(
      gfx::Point(min_x, min_y),
      gfx::Size(base::ClampSub(max_x, min_x), base::ClampSub(max_y, min_y)));

  SelectionRegion& selection_region = view_map_.at(view).selection_region;
  if (anchor_bound == selection_region.anchor &&
      focus_bound == selection_region.focus &&
      bounding_box_transformed == selection_region.bounding_box) {
    return;
  }

  selection_region.anchor = anchor_bound;
  selection_region.focus = focus_bound;
  selection_region.bounding_box = bounding_box_transformed;

  if (anchor_rect == focus_rect) {
    selection_region.caret_rect = transformed_anchor_rect;
  }
  selection_region.first_selection_rect = transformed_anchor_rect;

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
void TextInputManager::ImeCompositionRangeChanged(
    RenderWidgetHostViewBase* view,
    const gfx::Range& range,
    const std::optional<std::vector<gfx::Rect>>& character_bounds) {
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);

  if (character_bounds.has_value()) {
    CompositionRangeInfo& composition_range_info =
        view_map_.at(view).composition_range_info;
    composition_range_info.character_bounds.clear();

    gfx::Rect viewport_rect = GetRootOrFallbackViewportRect(view);
    // The values for the bounds should be converted to root view's coordinates
    // before being stored.
    for (auto& rect : character_bounds.value()) {
      gfx::Rect clamped_rect = rect;
      clamped_rect.set_origin(
          view->TransformPointToRootCoordSpace(clamped_rect.origin()));
      clamped_rect.AdjustToFit(viewport_rect);
      composition_range_info.character_bounds.emplace_back(clamped_rect);
    }

    composition_range_info.range.set_start(range.start());
    composition_range_info.range.set_end(range.end());
  }

  for (auto& observer : observer_list_) {
    observer.OnImeCompositionRangeChanged(this, view,
                                          character_bounds.has_value());
  }
}

void TextInputManager::SelectionChanged(RenderWidgetHostViewBase* view,
                                        const std::u16string& text,
                                        size_t offset,
                                        const gfx::Range& range) {
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);
  view_map_.at(view).text_selection.SetSelection(text, offset, range);
  for (auto& observer : observer_list_)
    observer.OnTextSelectionChanged(this, view);
}

void TextInputManager::Register(RenderWidgetHostViewBase* view) {
  CHECK(!IsRegistered(view), base::NotFatalUntil::M153);
  view_map_.try_emplace(view);
}

void TextInputManager::DidEnterBackForwardCache(
    RenderWidgetHostViewBase* view) {
  if (!IsRegistered(view) || active_view_ != view) {
    return;
  }
  // The view remains registered and its cached state is preserved across
  // BFCache, but it is no longer the active text-input target while the page is
  // frozen.
  active_view_ = nullptr;
  NotifyObserversAboutInputStateUpdate(view, true);
}

void TextInputManager::Unregister(RenderWidgetHostViewBase* view) {
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);

  view_map_.erase(view);

  if (active_view_ == view) {
    active_view_ = nullptr;
    NotifyObserversAboutInputStateUpdate(view, true);
  }
  view->DidUnregisterFromTextInputManager(this);
}

bool TextInputManager::IsRegistered(RenderWidgetHostViewBase* view) const {
  return view_map_.count(view) == 1;
}

bool TextInputManager::IsViewFocused(RenderWidgetHostViewBase* view) const {
  if (!view || !view->host()) {
    return false;
  }
  if (view->GetWidgetType() == WidgetType::kPopup) {
    // Popups themselves do not hold frame focus directly. Instead, delegate the
    // focus check to the frame that created the popup.
    if (auto creator_id = view->host()->GetPopupCreatorFrameId()) {
      RenderFrameHostImpl* creator_rfh =
          RenderFrameHostImpl::FromID(*creator_id);
      if (!creator_rfh) {
        return false;
      }
      // When site isolation is disabled (such as for subframes on Android),
      // child frames share the root RenderWidgetHostView with the main frame.
      // Checking only whether the creator's view is focused would incorrectly
      // return true even if the creator frame is not focused. Verify that the
      // creator frame itself is the currently focused frame in the frame tree.
      FrameTreeNode* focused_frame =
          creator_rfh->frame_tree_node()->frame_tree().GetFocusedFrame();
      RenderFrameHostImpl* focused_rfh =
          focused_frame ? focused_frame->current_frame_host()
                        : creator_rfh->GetMainFrame();
      if (creator_rfh != focused_rfh) {
        return false;
      }
      RenderWidgetHostViewBase* creator_view =
          static_cast<RenderWidgetHostViewBase*>(creator_rfh->GetView());
      return IsViewFocused(creator_view);
    }
    return false;
  }
  RenderWidgetHostViewBase* root_view = view->GetRootView();
  if (!root_view) {
    return false;
  }
  RenderWidgetHostImpl* focused_widget = root_view->GetFocusedWidget();
  if (!focused_widget) {
    return false;
  }
  return view->GetRenderWidgetHost() == focused_widget;
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
  return view_map_.size();
}

ui::TextInputType TextInputManager::GetTextInputTypeForViewForTesting(
    RenderWidgetHostViewBase* view) {
  CHECK(IsRegistered(view), base::NotFatalUntil::M153);
  return view_map_.at(view).text_input_state->type;
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

TextInputManager::ViewState::ViewState() = default;

TextInputManager::ViewState::~ViewState() = default;

TextInputManager::SelectionRegion::SelectionRegion() = default;

TextInputManager::SelectionRegion::SelectionRegion(
    const SelectionRegion& other) = default;

TextInputManager::SelectionRegion& TextInputManager::SelectionRegion::operator=(
    const SelectionRegion& other) = default;

bool TextInputManager::SelectionRegion::operator==(
    const SelectionRegion& other) const = default;

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
