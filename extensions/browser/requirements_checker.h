// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_REQUIREMENTS_CHECKER_H_
#define EXTENSIONS_BROWSER_REQUIREMENTS_CHECKER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/preload_check.h"

namespace content {
class GpuFeatureChecker;
}

namespace extensions {
class Extension;

// Validates the 'requirements' extension manifest field. This is an
// asynchronous process that involves several threads, but the public interface
// of this class (including constructor and destructor) must only be used on
// the UI thread.
class RequirementsChecker : public PreloadCheck {
 public:
  explicit RequirementsChecker(scoped_refptr<const Extension> extension);

  RequirementsChecker(const RequirementsChecker&) = delete;
  RequirementsChecker& operator=(const RequirementsChecker&) = delete;

  ~RequirementsChecker() override;

  // PreloadCheck:
  void Start(ResultCallback callback) override;
  // Joins multiple errors into a space-separated string.
  std::u16string GetErrorMessage() const override;

 private:
  // Callback for the GpuFeatureChecker.
  void VerifyWebGLAvailability(bool available);

  // Helper function to run the callback.
  void RunCallback();

  ResultCallback callback_;
  Errors errors_;

  base::WeakPtrFactory<RequirementsChecker> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_REQUIREMENTS_CHECKER_H_
