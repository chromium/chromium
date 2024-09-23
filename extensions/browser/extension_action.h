// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ACTION_H_
#define EXTENSIONS_BROWSER_EXTENSION_ACTION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/map_util.h"
#include "base/values.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

class GURL;

namespace gfx {
class Image;
class ImageSkia;
}  // namespace gfx

namespace extensions {
class Extension;
class IconImage;

// ExtensionAction encapsulates the state of a browser action or page action.
// Instances can have both global and per-tab state. If a property does not have
// a per-tab value, the global value is used instead.
class ExtensionAction {
 public:
  // The action that the UI should take after the ExtensionAction is clicked.
  enum class ShowAction {
    kNone,
    kShowPopup,
    kToggleSidePanel,
    // We don't need a SHOW_CONTEXT_MENU because that's handled separately in
    // the UI.
  };

  enum class IconParseResult {
    kSuccess,
    kDecodeFailure,
    kUnpickleFailure,
  };

  static extension_misc::ExtensionIcons ActionIconSize();

  // Returns the default icon to use when no other is available (the puzzle
  // piece).
  static gfx::Image FallbackIcon();

  // Use this ID to indicate the default state for properties that take a tab_id
  // parameter.
  static const int kDefaultTabId;

  ExtensionAction(const Extension& extension, const ActionInfo& manifest_data);

  ExtensionAction(const ExtensionAction&) = delete;
  ExtensionAction& operator=(const ExtensionAction&) = delete;

  ~ExtensionAction();

  // extension id
  const ExtensionId& extension_id() const { return extension_id_; }

  // What kind of action is this?
  ActionInfo::Type action_type() const { return action_type_; }

  ActionInfo::DefaultState default_state() const { return default_state_; }

  // Set the url which the popup will load when the user clicks this action's
  // icon.  Setting an empty URL will disable the popup for a given tab.
  void SetPopupUrl(int tab_id, const GURL& url);

  // Use HasPopup() to see if a popup should be displayed.
  bool HasPopup(int tab_id) const;

  // Get the URL to display in a popup.
  GURL GetPopupUrl(int tab_id) const;

  // Set this action's title on a specific tab.
  void SetTitle(int tab_id, const std::string& title) {
    SetValue(&title_, tab_id, title);
  }

  // If tab |tab_id| has a set title, return it.  Otherwise, return
  // the default title.
  std::string GetTitle(int tab_id) const { return GetValue(title_, tab_id); }

  // Icons are a bit different because the default value can be set to either a
  // bitmap or a path. However, conceptually, there is only one default icon.
  // Setting the default icon using a path clears the bitmap and vice-versa.
  // To retrieve the icon for the extension action, use
  // ExtensionActionIconFactory.

  // Set this action's icon bitmap on a specific tab.
  void SetIcon(int tab_id, const gfx::Image& image);

  // Tries to parse |*icon| from a dictionary {"19": imageData19, "38":
  // imageData38}, and returns the result of the parsing attempt.
  static IconParseResult ParseIconFromCanvasDictionary(
      const base::Value::Dict& dict,
      gfx::ImageSkia* icon);

  // Gets the icon that has been set using |SetIcon| for the tab.
  gfx::Image GetExplicitlySetIcon(int tab_id) const;

  // Sets the icon for a tab, in a way that can't be read by the extension's
  // javascript.  Multiple icons can be set at the same time; some icon with the
  // highest priority will be used.
  void DeclarativeSetIcon(int tab_id, int priority, const gfx::Image& icon);
  void UndoDeclarativeSetIcon(int tab_id, int priority, const gfx::Image& icon);

  const ExtensionIconSet* default_icon() const { return default_icon_.get(); }

  // Set this action's badge text on a specific tab.
  void SetBadgeText(int tab_id, const std::string& text) {
    SetValue(&badge_text_, tab_id, text);
  }

  // Clear this action's badge text on a specific tab.
  void ClearBadgeText(int tab_id) { badge_text_.erase(tab_id); }

  // Get the badge text that has been set using SetBadgeText for a tab, or the
  // default if no badge text was set.
  std::string GetExplicitlySetBadgeText(int tab_id) const {
    return GetValue(badge_text_, tab_id);
  }

  // Set this action's badge text color on a specific tab.
  void SetBadgeTextColor(int tab_id, SkColor text_color) {
    SetValue(&badge_text_color_, tab_id, text_color);
  }
  // Get the text color for a tab, or the default color if no text color
  // was set.
  SkColor GetBadgeTextColor(int tab_id) const {
    return GetValue(badge_text_color_, tab_id);
  }

  // Set this action's badge background color on a specific tab.
  void SetBadgeBackgroundColor(int tab_id, SkColor color) {
    SetValue(&badge_background_color_, tab_id, color);
  }
  // Get the badge background color for a tab, or the default if no color
  // was set.
  SkColor GetBadgeBackgroundColor(int tab_id) const {
    return GetValue(badge_background_color_, tab_id);
  }

  // Set this ExtensionAction's DNR matched action count on a specific tab.
  void SetDNRActionCount(int tab_id, int action_count) {
    SetValue(&dnr_action_count_, tab_id, action_count);
  }
  // Get this ExtensionAction's DNR matched action count on a specific tab.
  // Returns -1 if no entry is found.
  int GetDNRActionCount(int tab_id) const {
    return GetValue(dnr_action_count_, tab_id);
  }
  // Clear this ExtensionAction's DNR matched action count for all tabs.
  void ClearDNRActionCountForAllTabs() { dnr_action_count_.clear(); }

  // Get the badge text displayed for a tab, calculated based on both
  // |badge_text_| and |dnr_action_count_|. Returns in order of priority:
  // - GetExplicitlySetBadgeText(tab_id) if it exists for the |tab_id|
  // - GetDNRActionCount(tab_id) if there is at least one action for this tab
  // - The default badge text, if set, otherwise: an empty string.
  std::string GetDisplayBadgeText(int tab_id) const;

  // Set this action's badge visibility on a specific tab.  Returns true if
  // the visibility has changed.
  bool SetIsVisible(int tab_id, bool value);
  // The declarative appearance overrides a default appearance but is overridden
  // by an appearance set directly on the tab.
  void DeclarativeShow(int tab_id);
  void UndoDeclarativeShow(int tab_id);
  const gfx::Image GetDeclarativeIcon(int tab_id) const;

  // Get the badge visibility for a tab, or the default badge visibility
  // if none was set.
  // Gets the visibility of |tab_id|.  Returns the first of: a specific
  // visibility set on the tab; a declarative visibility set on the tab; the
  // default visibility set for all tabs; or |false|.  Don't return this
  // result to an extension's background page because the declarative state can
  // leak information about hosts the extension doesn't have permission to
  // access.
  bool GetIsVisible(int tab_id) const {
    return GetIsVisibleInternal(tab_id, /*include_declarative=*/true);
  }

  bool GetIsVisibleIgnoringDeclarative(int tab_id) const {
    return GetIsVisibleInternal(tab_id, /*include_declarative=*/false);
  }

  // Remove all tab-specific state.
  void ClearAllValuesForTab(int tab_id);

  // Sets the default IconImage for this action.
  void SetDefaultIconImage(std::unique_ptr<IconImage> icon_image);

  // Returns the image to use as the default icon for the action. Can only be
  // called after SetDefaultIconImage().
  gfx::Image GetDefaultIconImage() const;

  // Returns the placeholder image for the extension.
  gfx::Image GetPlaceholderIconImage() const;

  // Determine whether or not the ExtensionAction has a value set for the given
  // |tab_id| for each property.
  bool HasPopupUrl(int tab_id) const;
  bool HasTitle(int tab_id) const;
  bool HasBadgeText(int tab_id) const;
  bool HasBadgeBackgroundColor(int tab_id) const;
  bool HasBadgeTextColor(int tab_id) const;
  bool HasIsVisible(int tab_id) const;
  bool HasIcon(int tab_id) const;
  bool HasDNRActionCount(int tab_id) const;

  IconImage* default_icon_image() { return default_icon_image_.get(); }

  void SetDefaultIconForTest(std::unique_ptr<ExtensionIconSet> default_icon);

 private:
  // Populates the action from the |extension| and |manifest_data|, filling in
  // any missing values (like title or icons) as possible.
  void Populate(const Extension& extension, const ActionInfo& manifest_data);

  // Returns width of the current icon for tab_id.
  // TODO(tbarzic): The icon selection is done in ExtensionActionIconFactory.
  // We should probably move this there too.
  int GetIconWidth(int tab_id) const;

  // Returns whether the icon is visible on the given `tab`.
  // `include_declarative` indicates whether this method should take into
  // account declaratively-shown icons; this should only be true when the result
  // of this function is not delivered (directly or indirectly) to the
  // extension, since it can leak data about the page in the tab.
  bool GetIsVisibleInternal(int tab_id, bool include_declarative) const;

  template <class T>
  struct ValueTraits {
    static T CreateEmpty() { return T(); }
  };

  template <class T>
  void SetValue(std::map<int, T>* map, int tab_id, const T& val) {
    (*map)[tab_id] = val;
  }

  template <class T>
  T GetValue(const std::map<int, T>& map, int tab_id) const {
    if (const T* tab_value = base::FindOrNull(map, tab_id)) {
      return *tab_value;
    } else if (const T* default_value = base::FindOrNull(map, kDefaultTabId)) {
      return *default_value;
    } else {
      return ValueTraits<T>::CreateEmpty();
    }
  }

  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  const ExtensionId extension_id_;

  // The name of the extension.
  const std::string extension_name_;

  const ActionInfo::Type action_type_;
  // The default state of the action.
  const ActionInfo::DefaultState default_state_;

  // Each of these data items can have both a global state (stored with the key
  // kDefaultTabId), or tab-specific state (stored with the tab_id as the key).
  std::map<int, GURL> popup_url_;
  std::map<int, std::string> title_;
  std::map<int, gfx::Image> icon_;
  std::map<int, std::string> badge_text_;
  std::map<int, SkColor> badge_background_color_;
  std::map<int, SkColor> badge_text_color_;
  std::map<int, bool> is_visible_;

  // Declarative state exists for two reasons: First, we need to hide it from
  // the extension's background/event page to avoid leaking data from hosts the
  // extension doesn't have permission to access.  Second, the action's state
  // gets both reset and given its declarative values in response to a
  // WebContentsObserver::DidNavigateMainFrame event, and there's no way to set
  // those up to be called in the right order.

  // Maps tab_id to the number of active (applied-but-not-reverted)
  // declarativeContent.ShowAction actions.
  std::map<int, int> declarative_show_count_;

  // declarative_icon_[tab_id][declarative_rule_priority] is a vector of icon
  // images that are currently in effect
  std::map<int, std::map<int, std::vector<gfx::Image>>> declarative_icon_;

  // Maps tab_id to the number of actions taken based on declarative net request
  // rule matches on incoming requests. Overrides the default |badge_text_| for
  // this extension if it has opted into setting the action count as badge text.
  std::map<int, int> dnr_action_count_;

  // ExtensionIconSet containing paths to bitmaps from which default icon's
  // image representations will be selected.
  std::unique_ptr<ExtensionIconSet> default_icon_;

  // The default icon image, if |default_icon_| exists. Set via
  // SetDefaultIconImage(). Since IconImages depend upon BrowserContexts, we
  // don't have the ExtensionAction load it directly to keep this class's
  // knowledge limited.
  std::unique_ptr<IconImage> default_icon_image_;

  // The lazily-initialized image for a placeholder icon, in the event that the
  // extension doesn't have its own icon. (Mutable to allow lazy init in
  // GetDefaultIconImage().)
  mutable gfx::Image placeholder_icon_image_;

  // The id for the ExtensionAction, for example: "RssPageAction". This is
  // needed for compat with an older version of the page actions API.
  std::string id_;
};

template <>
struct ExtensionAction::ValueTraits<int> {
  static int CreateEmpty() { return -1; }
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ACTION_H_
