// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/accessibility_window_info_data_wrapper.h"
#include "base/memory/raw_ptr.h"

#include "base/notreached.h"
#include "chrome/grit/generated_resources.h"
#include "services/accessibility/android/android_accessibility_util.h"
#include "services/accessibility/android/ax_tree_source_android.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ax::android {

AccessibilityWindowInfoDataWrapper::AccessibilityWindowInfoDataWrapper(
    AXTreeSourceAndroid* tree_source,
    mojom::AccessibilityWindowInfoData* window)
    : AccessibilityInfoDataWrapper(tree_source), window_ptr_(window) {}

AccessibilityWindowInfoDataWrapper::~AccessibilityWindowInfoDataWrapper() =
    default;

bool AccessibilityWindowInfoDataWrapper::IsNode() const {
  return false;
}

mojom::AccessibilityNodeInfoData* AccessibilityWindowInfoDataWrapper::GetNode()
    const {
  return nullptr;
}

mojom::AccessibilityWindowInfoData*
AccessibilityWindowInfoDataWrapper::GetWindow() const {
  return window_ptr_;
}

int32_t AccessibilityWindowInfoDataWrapper::GetId() const {
  return window_ptr_->window_id;
}

const gfx::Rect AccessibilityWindowInfoDataWrapper::GetBounds() const {
  return window_ptr_->bounds_in_screen;
}

bool AccessibilityWindowInfoDataWrapper::IsVisibleToUser() const {
  return true;
}

bool AccessibilityWindowInfoDataWrapper::IsWebNode() const {
  return false;
}

bool AccessibilityWindowInfoDataWrapper::IsIgnored() const {
  return false;
}

bool AccessibilityWindowInfoDataWrapper::IsImportantInAndroid() const {
  return true;
}

bool AccessibilityWindowInfoDataWrapper::IsFocusableInFullFocusMode() const {
  // Windows are too generic to be Accessibility focused in Chrome, although
  // they can be Accessibility focused in Android by virtue of having
  // accessibility focus on nodes within themselves.
  return false;
}

bool AccessibilityWindowInfoDataWrapper::IsAccessibilityFocusableContainer()
    const {
  return tree_source_->GetRoot()->GetId() == GetId();
}

void AccessibilityWindowInfoDataWrapper::PopulateAXRole(
    ui::AXNodeData* out_data) const {
  if (tree_source_->is_notification()) {
    // Notification window doesn't have window type. As the notification window
    // is a part of notification center UI, use generic container role.
    out_data->role = ax::mojom::Role::kGenericContainer;
    return;
  }
  switch (window_ptr_->window_type) {
    case mojom::AccessibilityWindowType::TYPE_ACCESSIBILITY_OVERLAY:
      out_data->role = ax::mojom::Role::kWindow;
      return;
    case mojom::AccessibilityWindowType::TYPE_APPLICATION:
      if (tree_source_->GetRoot()->GetId() == GetId()) {
        // Root of this task.
        out_data->role = ax::mojom::Role::kApplication;
      } else {
        // A part of the main window.
        out_data->role = ax::mojom::Role::kGenericContainer;
      }
      return;
    case mojom::AccessibilityWindowType::TYPE_INPUT_METHOD:
      out_data->role = ax::mojom::Role::kKeyboard;
      return;
    case mojom::AccessibilityWindowType::TYPE_SPLIT_SCREEN_DIVIDER:
      // A system window used to divide the screen in split-screen mode. This
      // type of window is present only in split-screen mode.
      out_data->role = ax::mojom::Role::kSplitter;
      return;
    case mojom::AccessibilityWindowType::TYPE_SYSTEM:
      out_data->role = ax::mojom::Role::kWindow;
      return;
    case mojom::AccessibilityWindowType::INVALID_ENUM_VALUE:
      NOTREACHED_IN_MIGRATION();
      return;
  }
}

void AccessibilityWindowInfoDataWrapper::PopulateAXState(
    ui::AXNodeData* out_data) const {
  // ARC++ window states are not reflected in ax::mojom::State, and for the
  // most part aren't needed.
}

void AccessibilityWindowInfoDataWrapper::Serialize(
    ui::AXNodeData* out_data) const {
  AccessibilityInfoDataWrapper* root = tree_source_->GetRoot();
  if (!root) {
    return;
  }

  AccessibilityInfoDataWrapper::Serialize(out_data);

  // String properties.
  const std::string name = ComputeAXName(true);
  if (!name.empty()) {
    out_data->SetName(name);
    out_data->SetNameFrom(ax::mojom::NameFrom::kTitle);
  }

  if (GetProperty(mojom::AccessibilityWindowBooleanProperty::
                      IN_PICTURE_IN_PICTURE_MODE)) {
    out_data->AddStringAttribute(
        ax::mojom::StringAttribute::kDescription,
        l10n_util::GetStringUTF8(IDS_ARC_ACCESSIBILITY_WINDOW_TITLE_IN_PIP));
  }

  if (root->GetId() == GetId()) {
    // Make the root window of each ARC task modal unless it's notification.
    if (!tree_source_->is_notification()) {
      out_data->AddBoolAttribute(ax::mojom::BoolAttribute::kModal, true);
    }

    // Focusable in Android simply means a node within the window is focusable.
    // The window itself is not focusable in Android, but ChromeVox sets the
    // focus to the entire window, explicitly specify this.
    out_data->AddState(ax::mojom::State::kFocusable);
  }

  // Note that not all properties are currently used in Chrome Accessibility.
}

std::string AccessibilityWindowInfoDataWrapper::ComputeAXName(
    bool do_recursive) const {
  std::string title;
  GetProperty(mojom::AccessibilityWindowStringProperty::TITLE, &title);
  return title;
}

void AccessibilityWindowInfoDataWrapper::GetChildren(
    std::vector<raw_ptr<AccessibilityInfoDataWrapper, VectorExperimental>>*
        children) const {
  // Populate the children vector by combining the child window IDs with the
  // root node ID.
  if (window_ptr_->int_list_properties) {
    const auto& it = window_ptr_->int_list_properties->find(
        mojom::AccessibilityWindowIntListProperty::CHILD_WINDOW_IDS);
    if (it != window_ptr_->int_list_properties->end()) {
      for (const int32_t id : it->second) {
        auto* child = tree_source_->GetFromId(id);
        if (child != nullptr) {
          children->push_back(child);
        } else {
          LOG(WARNING) << "Unexpected nullptr found while GetChildren";
        }
      }
    }
  }

  if (window_ptr_->root_node_id) {
    auto* child_node = tree_source_->GetFromId(window_ptr_->root_node_id);
    if (child_node != nullptr) {
      children->push_back(child_node);
    } else {
      LOG(WARNING) << "Unexpected nullptr found while populating root node for "
                      "GetChildren";
    }
  }

  for (int32_t vitual_child_id : virtual_child_ids_) {
    auto* child_node = tree_source_->GetFromId(vitual_child_id);
    if (child_node != nullptr) {
      children->push_back(child_node);
    } else {
      LOG(WARNING)
          << "Unexpected nullptr found while populating virtual child node for "
             "GetChildren";
    }
  }
}

int32_t AccessibilityWindowInfoDataWrapper::GetWindowId() const {
  return window_ptr_->window_id;
}

void AccessibilityWindowInfoDataWrapper::AddVirtualChild(int32_t child_id) {
  if (base::ranges::find(virtual_child_ids_, child_id) !=
      virtual_child_ids_.end()) {
    LOG(ERROR) << "Given child id already exists as a virtual child.";
  } else {
    virtual_child_ids_.push_back(child_id);
  }
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowBooleanProperty prop) const {
  return GetBooleanProperty(window_ptr_.get(), prop);
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowIntProperty prop,
    int32_t* out_value) const {
  return ax::android::GetProperty(window_ptr_->int_properties, prop, out_value);
}

bool AccessibilityWindowInfoDataWrapper::HasProperty(
    mojom::AccessibilityWindowStringProperty prop) const {
  return ax::android::HasProperty(window_ptr_->string_properties, prop);
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowStringProperty prop,
    std::string* out_value) const {
  return ax::android::GetProperty(window_ptr_->string_properties, prop,
                                  out_value);
}

bool AccessibilityWindowInfoDataWrapper::GetProperty(
    mojom::AccessibilityWindowIntListProperty prop,
    std::vector<int32_t>* out_value) const {
  return ax::android::GetProperty(window_ptr_->int_list_properties, prop,
                                  out_value);
}

}  // namespace ax::android
