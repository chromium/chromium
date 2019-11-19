// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_

#include <set>

#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/browser/content_verify_job.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class Version;
}

namespace extensions {

class Extension;

// This is an interface for clients that want to use a ContentVerifier.
class ContentVerifierDelegate {
 public:
  // Types of hash sources used for content verification of an extension.
  enum class VerifierSourceType {
    // Use no hashes for verification, this effectively means the extension
    // won't be verified.
    NONE,

    // Use unsigned local hashes (computed_hashes.json) only and not
    // verified_contents.json.
    UNSIGNED_HASHES,

    // Use signed hashes (verified_contents.json).
    // Note that GetPublicKey and GetSignatureFetchUrl would be required for
    // this.
    SIGNED_HASHES,
  };

  virtual ~ContentVerifierDelegate() {}

  // Returns verification source type for |extension|.
  virtual VerifierSourceType GetVerifierSourceType(
      const Extension& extension) = 0;

  // Returns the public key to use for validating signatures.
  virtual ContentVerifierKey GetPublicKey() = 0;

  // Returns a URL that can be used to fetch the verified_contents.json
  // containing signatures for the given extension id/version pair.
  virtual GURL GetSignatureFetchUrl(const std::string& extension_id,
                                    const base::Version& version) = 0;

  // Returns the set of file paths for images used within the browser process.
  // (These may get transcoded during the install process).
  virtual std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) = 0;

  // Called when the content verifier detects that a read of a file inside an
  // extension did not match its expected hash.
  virtual void VerifyFailed(const std::string& extension_id,
                            ContentVerifyJob::FailureReason reason) = 0;

  // Called when ExtensionSystem is shutting down.
  virtual void Shutdown() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_DELEGATE_H_
