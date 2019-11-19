// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context_set_iterable.h"

namespace extensions {

void ScriptContextSetIterable::ForEach(
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  ForEach(std::string(), render_frame, callback);
}

void ScriptContextSetIterable::ForEach(
    const std::string& extension_id,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  ForEach(extension_id, nullptr, callback);
}

}  // namespace extensions
