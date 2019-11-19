// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/widget_messages.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

namespace {

bool ShouldUpdateTextInputState(const content::TextInputState& old_state,
                                 const content::TextInputState& new_state) {
#if defined(USE_AURA)
  return old_state.type != new_state.type || old_state.mode != new_state.mode ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#elif defined(OS_MACOSX)
  return old_state.type != new_state.type ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#elif defined(OS_ANDROID)
  // On Android, TextInputState update is sent only if there is some change in
  // the state. So the new state is always different.
  return true;
#else
  NOTREACHED();
  return true;
#endif
}

}  // namespace

TextInputManager::TextInputManager(bool should_do_learning)
    : active_view_(nullptr), should_do_learning_(should_do_learning) {}

TextInputManager::~TextInputManager() {
  // If there is an active view, we should unregister it first so that the
  // the tab's top-level RWHV will be notified about |TextInputState.type|
  // resetting to none (i.e., we do not have an active RWHV anymore).
  if (active_view_)
    Unregister(active_view_);

  // Unregister all the remaining views.
  std::vector<RenderWidgetHostViewBase*> views;
  for (auto pair : text_input_state_map_)
    views.push_back(pair.first);

  for (auto* view : views)
    Unregister(view);
}

RenderWidgetHostImpl* TextInputManager::GetActiveWidget() const {
  return !!active_view_ ? static_cast<RenderWidgetHostImpl*>(
                              active_view_->GetRenderWidgetHost())
                        : nullptr;
}

const TextInputState* TextInputManager::GetTextInputState() const {
  return !!active_view_ ? &text_input_state_map_.at(active_view_) : nullptr;
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

void TextInputManager::UpdateTextInputState(
    RenderWidgetHostViewBase* view,
    const TextInputState& text_input_state) {
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
#if !defined(OS_ANDROID)
    return;
#endif
  }

  // Since |view| is registered, we already have a previous value for its
  // TextInputState.
  bool changed = ShouldUpdateTextInputState(text_input_state_map_[view],
                                            text_input_state);

  text_input_state_map_[view] = text_input_state;

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
      text_input_state_map_[active_view_].type = ui::TEXT_INPUT_TYPE_NONE;
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
    const WidgetHostMsg_SelectionBounds_Params& params) {
  DCHECK(IsRegistered(view));
  // Converting the anchor point to root's coordinate space (for child frame
  // views).
  gfx::Point anchor_origin_transformed =
      view->TransformPointToRootCoordSpace(params.anchor_rect.origin());

  gfx::SelectionBound anchor_bound, focus_bound;

  anchor_bound.SetEdge(gfx::PointF(anchor_origin_transformed),
                       gfx::PointF(view->TransformPointToRootCoordSpace(
                           params.anchor_rect.bottom_left())));
  focus_bound.SetEdge(gfx::PointF(view->TransformPointToRootCoordSpace(
                          params.focus_rect.origin())),
                      gfx::PointF(view->TransformPointToRootCoordSpace(
                          params.focus_rect.bottom_left())));

  if (params.anchor_rect == params.focus_rect) {
    anchor_bound.set_type(gfx::SelectionBound::CENTER);
    focus_bound.set_type(gfx::SelectionBound::CENTER);
  } else {
    // Whether text is LTR at the anchor handle.
    bool anchor_LTR = params.anchor_dir == blink::kWebTextDirectionLeftToRight;
    // Whether text is LTR at the focus handle.
    bool focus_LTR = params.focus_dir == blink::kWebTextDirectionLeftToRight;

    if ((params.is_anchor_first && anchor_LTR) ||
        (!params.is_anchor_first && !anchor_LTR)) {
      anchor_bound.set_type(gfx::SelectionBound::LEFT);
    } else {
      anchor_bound.set_type(gfx::SelectionBound::RIGHT);
    }
    if ((params.is_anchor_first && focus_LTR) ||
        (!params.is_anchor_first && !focus_LTR)) {
      focus_bound.set_type(gfx::SelectionBound::RIGHT);
    } else {
      focus_bound.set_type(gfx::SelectionBound::LEFT);
    }
  }

  if (anchor_bound == selection_region_map_[view].anchor &&
      focus_bound == selection_region_map_[view].focus)
    return;

  selection_region_map_[view].anchor = anchor_bound;
  selection_region_map_[view].focus = focus_bound;

  if (params.anchor_rect == params.focus_rect) {
    selection_region_map_[view].caret_rect.set_origin(
        anchor_origin_transformed);
    selection_region_map_[view].caret_rect.set_size(params.anchor_rect.size());
  }
  selection_region_map_[view].first_selection_rect.set_origin(
      anchor_origin_transformed);
  selection_region_map_[view].first_selection_rect.set_size(
      params.anchor_rect.size());

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
    const std::vector<gfx::Rect>& character_bounds) {
  DCHECK(IsRegistered(view));
  composition_range_info_map_[view].character_bounds.clear();

  // The values for the bounds should be converted to root view's coordinates
  // before being stored.
  for (auto rect : character_bounds) {
    composition_range_info_map_[view].character_bounds.emplace_back(gfx::Rect(
        view->TransformPointToRootCoordSpace(rect.origin()), rect.size()));
  }

  composition_range_info_map_[view].range.set_start(range.start());
  composition_range_info_map_[view].range.set_end(range.end());

  for (auto& observer : observer_list_)
    observer.OnImeCompositionRangeChanged(this, view);
}

void TextInputManager::SelectionChanged(RenderWidgetHostViewBase* view,
                                        const base::string16& text,
                                        size_t offset,
                                        const gfx::Range& range) {
  DCHECK(IsRegistered(view));
  text_selection_map_[view].SetSelection(text, offset, range);
  for (auto& observer : observer_list_)
    observer.OnTextSelectionChanged(this, view);
}

void TextInputManager::Register(RenderWidgetHostViewBase* view) {
  DCHECK(!IsRegistered(view));

  text_input_state_map_[view] = TextInputState();
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
  return text_input_state_map_[view].type;
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

TextInputManager::SelectionRegion::SelectionRegion() {}

TextInputManager::SelectionRegion::SelectionRegion(
    const SelectionRegion& other) = default;

TextInputManager::CompositionRangeInfo::CompositionRangeInfo() {}

TextInputManager::CompositionRangeInfo::CompositionRangeInfo(
    const CompositionRangeInfo& other) = default;

TextInputManager::CompositionRangeInfo::~CompositionRangeInfo() {}

TextInputManager::TextSelection::TextSelection()
    : offset_(0), range_(gfx::Range::InvalidRange()) {}

TextInputManager::TextSelection::TextSelection(const TextSelection& other) =
    default;

TextInputManager::TextSelection::~TextSelection() {}

void TextInputManager::TextSelection::SetSelection(const base::string16& text,
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
