// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PRELOAD_CHECK_H_
#define EXTENSIONS_BROWSER_PRELOAD_CHECK_H_

#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace extensions {

class Extension;

// Encapsulates a possibly asynchronous operation to verify whether a
// precondition holds for loading the given extension.
class PreloadCheck {
 public:
  // These enumerators should only be referred to by name, so it is safe to
  // insert or remove values as necessary.
  enum Error {
    NONE,
    BLOCKLISTED_ID,
    BLOCKLISTED_UNKNOWN,
    DISALLOWED_BY_POLICY,
    WEBGL_NOT_SUPPORTED,
    WINDOW_SHAPE_NOT_SUPPORTED,
  };

  using Errors = std::set<Error>;
  using ResultCallback = base::OnceCallback<void(const Errors&)>;

  explicit PreloadCheck(scoped_refptr<const Extension> extension);
  virtual ~PreloadCheck();

  // This function must be called on the UI thread. The callback also occurs on
  // the UI thread.
  virtual void Start(ResultCallback callback) = 0;

  // Subclasses may provide an error message.
  virtual std::u16string GetErrorMessage() const;

  const Extension* extension() { return extension_.get(); }

 private:
  // The extension to check.
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(PreloadCheck);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PRELOAD_CHECK_H_
