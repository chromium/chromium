// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_ITERABLE_H_
#define EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_ITERABLE_H_

#include <string>

#include "base/callback_forward.h"

namespace content {
class RenderFrame;
}

namespace extensions {
class ScriptContext;

// Iterable base class to iterate over a ScriptContextSet.
class ScriptContextSetIterable {
 public:
  // Synchronously runs |callback| with each ScriptContext that belongs to
  // |extension_id| in |render_frame|.
  //
  // An empty |extension_id| will match all extensions, and a null
  // |render_frame| will match all render views, but try to use the inline
  // variants of these methods instead.
  virtual void ForEach(
      const std::string& extension_id,
      content::RenderFrame* render_frame,
      const base::RepeatingCallback<void(ScriptContext*)>& callback) = 0;

  // ForEach which matches all extensions.
  void ForEach(content::RenderFrame* render_frame,
               const base::RepeatingCallback<void(ScriptContext*)>& callback);

  // ForEach which matches all render views.
  void ForEach(const std::string& extension_id,
               const base::RepeatingCallback<void(ScriptContext*)>& callback);

  virtual ~ScriptContextSetIterable() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_CONTEXT_SET_ITERABLE_H_
