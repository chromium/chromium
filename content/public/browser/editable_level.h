// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_EDITABLE_LEVEL_H_
#define CONTENT_PUBLIC_BROWSER_EDITABLE_LEVEL_H_

namespace content {

// Distinguishes whether an element is editable, and if so, whether it supports
// rich text formatting.
enum class EditableLevel {
  kNotEditable,
  kPlaintextEditable,
  kRichlyEditable,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_EDITABLE_LEVEL_H_
