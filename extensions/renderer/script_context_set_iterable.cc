// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/script_context_set_iterable.h"

#include "extensions/common/mojom/host_id.mojom.h"

namespace extensions {

void ScriptContextSetIterable::ForEach(
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  ForEach(mojom::HostID(), render_frame, callback);
}

void ScriptContextSetIterable::ForEach(
    const mojom::HostID& host_id,
    const base::RepeatingCallback<void(ScriptContext*)>& callback) {
  ForEach(host_id, nullptr, callback);
}

}  // namespace extensions
