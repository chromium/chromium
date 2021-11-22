// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_ANDROID_EXTERNAL_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_ANDROID_EXTERNAL_H_

#include "content/browser/accessibility/accessibility_tree_formatter_android.h"
#include "content/common/content_export.h"

namespace content {

class BrowserAccessibility;

class CONTENT_EXPORT AccessibilityTreeFormatterAndroidExternal
    : public AccessibilityTreeFormatterAndroid {
 public:
  AccessibilityTreeFormatterAndroidExternal();
  ~AccessibilityTreeFormatterAndroidExternal() override;

  base::Value BuildTree(ui::AXPlatformNodeDelegate* root) const override;

 private:
  void RecursiveBuildTree(const BrowserAccessibility& node,
                          base::DictionaryValue* dict) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_FORMATTER_ANDROID_EXTERNAL_H_
