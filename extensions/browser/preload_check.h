// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PRELOAD_CHECK_H_
#define EXTENSIONS_BROWSER_PRELOAD_CHECK_H_

#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace extensions {

class Extension;

// Encapsulates a possibly asynchronous operation to verify whether a
// precondition holds for loading the given extension.
class PreloadCheck {
 public:
  // These enumerators should only be referred to by name, so it is safe to
  // insert or remove values as necessary.
  enum class Error {
    kBlocklistedId,
    kBlocklistedUnknown,
    kDisallowedByPolicy,
    kWebglNotSupported,
  };

  using Errors = std::set<Error>;
  using ResultCallback = base::OnceCallback<void(const Errors&)>;

  explicit PreloadCheck(scoped_refptr<const Extension> extension);

  PreloadCheck(const PreloadCheck&) = delete;
  PreloadCheck& operator=(const PreloadCheck&) = delete;

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
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PRELOAD_CHECK_H_
