// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_MANAGER_H__
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_MANAGER_H__

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/selection_bound.h"

namespace content {

class RenderWidgetHostImpl;
class RenderWidgetHostViewBase;

// A class which receives updates of TextInputState from multiple sources and
// decides what the new TextInputState is. It also notifies the observers when
// text input state is updated.
class CONTENT_EXPORT TextInputManager {
 public:
  // The tab's top-level RWHV should be an observer of TextInputManager to get
  // notifications about changes in TextInputState or other IME related state
  // for child frames.
  class CONTENT_EXPORT Observer {
   public:
    // Called when a view has called UpdateTextInputState on TextInputManager.
    // If the call has led to a change in TextInputState, |did_update_state| is
    // true. In some plaforms, we need this update even when the state has not
    // changed (e.g., Aura for updating IME). Also note that |updated_view| is
    // the view which has most recently received an update in TextInputState.
    // |updated_view| should not be used to obtain any IME state since this
    // observer method might have been called in the process of unregistering
    // |active_view_| from TextInputManager (which in turn is a result of either
    // destroying |active_view_| or TextInputManager).
    virtual void OnUpdateTextInputStateCalled(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view,
        bool did_update_state) {}
    // Called when |updated_view| has called ImeCancelComposition on
    // TextInputManager.
    virtual void OnImeCancelComposition(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view) {}
    // Called when |updated_view| has changed its SelectionRegion.
    virtual void OnSelectionBoundsChanged(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view) {}
    // Called when |updated_view| has changed its CompositionRangeInfo.
    // |character_bounds_changed| marks whether the current
    // CompositionRangeInfo::character_bounds should be updated.
    // |line_bounds| (used by Android) is an optional list of line rectangles.
    // If it has no value, no update is required.
    virtual void OnImeCompositionRangeChanged(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view,
        bool character_bounds_changed,
        const std::optional<std::vector<gfx::Rect>>& line_bounds) {}
    // Called when the text selection for the |updated_view_| has changed.
    virtual void OnTextSelectionChanged(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view) {}
  };

  // Text selection bounds.
  struct SelectionRegion {
    SelectionRegion();
    SelectionRegion(const SelectionRegion& other);
    SelectionRegion& operator=(const SelectionRegion& other);

    // The begining of the selection region.
    gfx::SelectionBound anchor;
    // The end of the selection region (caret position).
    gfx::SelectionBound focus;

    // The bounding box of the selection region.
    gfx::Rect bounding_box;

    // The following variables are only used on Mac platform.
    // The current caret bounds.
    gfx::Rect caret_rect;
    // The current first selection bounds.
    gfx::Rect first_selection_rect;
  };

  // Composition range information.
  struct CompositionRangeInfo {
    CompositionRangeInfo();
    CompositionRangeInfo(const CompositionRangeInfo& other);
    ~CompositionRangeInfo();

    std::vector<gfx::Rect> character_bounds;
    gfx::Range range;
  };

  // This class is used to store text selection information for views. The text
  // selection information includes a range around the selected (highlighted)
  // text which is defined by an offset from the beginning of the page/frame,
  // a range for the selection, and the text including the selection which
  // might include several characters before and after it.
  class TextSelection {
   public:
    TextSelection();
    TextSelection(const TextSelection& other);
    ~TextSelection();

    void SetSelection(const std::u16string& text,
                      size_t offset,
                      const gfx::Range& range);

    const std::u16string& selected_text() const { return selected_text_; }
    size_t offset() const { return offset_; }
    const gfx::Range& range() const { return range_; }
    const std::u16string& text() const { return text_; }

   private:
    // The offset of the text stored in |text| relative to the start of the web
    // page.
    size_t offset_;

    // The range of the selection in the page (highlighted text).
    gfx::Range range_;

    // The highlighted text which is the portion of |text_| marked by |offset_|
    // and |range_|. It will be an empty string if either |text_| or |range_|
    // are empty of this selection information is invalid (i.e., |range_| does
    // not cover any of |text_|.
    std::u16string selected_text_;

    // Part of the text on the page which includes the highlighted text plus
    // possibly several characters before and after it.
    std::u16string text_;
  };

  TextInputManager();

  TextInputManager(const TextInputManager&) = delete;
  TextInputManager& operator=(const TextInputManager&) = delete;

  ~TextInputManager();

  // Returns the currently active widget, i.e., the RWH which is associated with
  // |active_view_|.
  RenderWidgetHostImpl* GetActiveWidget() const;

  // ---------------------------------------------------------------------------
  // The following methods can be used to obtain information about IME-related
  // state for the active RenderWidgetHost. If the active widget is nullptr, the
  // methods below will return nullptr as well.
  // Users of these methods should not hold on to the pointers as they become
  // dangling if the TextInputManager or |active_view_| are destroyed.

  // Returns the currently stored TextInputState for |active_view_|. A state of
  // nullptr can be interpreted as a ui::TextInputType of
  // ui::TEXT_INPUT_TYPE_NONE.
  const ui::mojom::TextInputState* GetTextInputState() const;

  // Returns the current autocorrect range, or an empty range if no autocorrect
  // range is currently present.
  gfx::Range GetAutocorrectRange() const;

  // Returns the grammar fragment which contains |range|. If non-existent,
  // returns nullopt.
  std::optional<ui::GrammarFragment> GetGrammarFragment(gfx::Range range) const;

  // Returns the selection bounds information for |view|. If |view| == nullptr,
  // it will return the corresponding information for |active_view_| or nullptr
  // if there are no active views.
  const SelectionRegion* GetSelectionRegion(
      RenderWidgetHostViewBase* view = nullptr) const;

  // Returns the composition range and character bounds information for the
  // |active_view_|. Returns nullptr If |active_view_| == nullptr.
  const TextInputManager::CompositionRangeInfo* GetCompositionRangeInfo() const;

  // The following method returns the text selection state for the given |view|.
  // If |view| == nullptr, it will assume |active_view_| and return its state.
  // In the case of |active_view_| == nullptr, the method will return nullptr.
  const TextSelection* GetTextSelection(
      RenderWidgetHostViewBase* view = nullptr) const;

  // Returns the bounds of the text control in the root frame.
  const std::optional<gfx::Rect> GetTextControlBounds() const;

  // Returns the bounds of the selected text in the root frame.
  const std::optional<gfx::Rect> GetTextSelectionBounds() const;

  // ---------------------------------------------------------------------------
  // The following methods are called by RWHVs on the tab to update their IME-
  // related state.

  // Updates the TextInputState for |view|.
  void UpdateTextInputState(RenderWidgetHostViewBase* view,
                            const ui::mojom::TextInputState& state);

  // The current IME composition has been cancelled on the renderer side for
  // the widget corresponding to |view|.
  void ImeCancelComposition(RenderWidgetHostViewBase* view);

  // Updates the selection bounds for the |view|. In Aura, selection bounds are
  // used to provide the InputMethod with the position of the caret, e.g., in
  // setting the position of the ui::ImeWindow.
  void SelectionBoundsChanged(RenderWidgetHostViewBase* view,
                              const gfx::Rect& anchor_rect,
                              base::i18n::TextDirection anchor_dir,
                              const gfx::Rect& focus_rect,
                              base::i18n::TextDirection focus_dir,
                              const gfx::Rect& bounding_box,
                              bool is_anchor_first);

  // Notify observers that the selection bounds have been updated. This is also
  // called when a view with a selection is reactivated.
  void NotifySelectionBoundsChanged(RenderWidgetHostViewBase* view);

  // Called when the composition range and/or character bounds have changed.
  void ImeCompositionRangeChanged(
      RenderWidgetHostViewBase* view,
      const gfx::Range& range,
      const std::optional<std::vector<gfx::Rect>>& character_bounds,
      const std::optional<std::vector<gfx::Rect>>& line_bounds);

  // Updates the new text selection information for the |view|.
  void SelectionChanged(RenderWidgetHostViewBase* view,
                        const std::u16string& text,
                        size_t offset,
                        const gfx::Range& range);

  // Registers the given |view| for tracking its TextInputState. This is called
  // by any view which has updates in its TextInputState (whether tab's RWHV or
  // that of a child frame). The |view| must unregister itself before being
  // destroyed (i.e., call TextInputManager::Unregister).
  void Register(RenderWidgetHostViewBase* view);

  // Clears the TextInputState from the |view|. If |view == active_view_|, this
  // call will lead to a TextInputState update since the TextInputState.type
  // should be reset to none.
  void Unregister(RenderWidgetHostViewBase* view);

  // Returns true if |view| is already registered.
  bool IsRegistered(RenderWidgetHostViewBase* view) const;

  // Add and remove observers for notifications regarding updates in the
  // TextInputState. Clients must be sure to remove themselves before they go
  // away.
  // Only the tab's RWHV should observer TextInputManager. In tests, however,
  // in addition to the tab's RWHV, one or more test observers might observe
  // TextInputManager.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(Observer* observer) const;

  RenderWidgetHostViewBase* active_view_for_testing() { return active_view_; }
  size_t GetRegisteredViewsCountForTesting();
  ui::TextInputType GetTextInputTypeForViewForTesting(
      RenderWidgetHostViewBase* view);
  const gfx::Range* GetCompositionRangeForTesting() const;

 private:
  // This class is used to create maps which hold specific IME state for a
  // view.
  template <class Value>
  class ViewMap : public std::unordered_map<RenderWidgetHostViewBase*, Value> {
  };

  void NotifyObserversAboutInputStateUpdate(RenderWidgetHostViewBase* view,
                                            bool did_update_state);

  // The view with active text input state, i.e., a focused <input> element.
  // It will be nullptr if no such view exists. Note that the active view
  // cannot have a |TextInputState.type| of ui::TEXT_INPUT_TYPE_NONE.
  raw_ptr<RenderWidgetHostViewBase> active_view_;

  // The following maps track corresponding IME state for views. For each view,
  // the values in the map are initialized and cleared in Register and
  // Unregister methods, respectively.
  ViewMap<ui::mojom::TextInputStatePtr> text_input_state_map_;
  ViewMap<SelectionRegion> selection_region_map_;
  ViewMap<CompositionRangeInfo> composition_range_info_map_;
  ViewMap<TextSelection> text_selection_map_;

  base::ObserverList<Observer>::Unchecked observer_list_;
};
}

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_MANAGER_H__
