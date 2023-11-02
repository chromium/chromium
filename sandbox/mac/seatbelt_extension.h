// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_MAC_SEATBELT_EXTENSION_H_
#define SANDBOX_MAC_SEATBELT_EXTENSION_H_

#include "sandbox/mac/seatbelt_export.h"

#include <stddef.h>

#include <memory>
#include <string>

namespace sandbox {

class SeatbeltExtensionToken;

// A SeatbeltExtension allows one process with access to resources to provide
// fine-grained extensions/allowances to another process' sandbox policy at
// run time. An extension can be issued by the privileged process, generating
// a token that can be sent over IPC. The receiving process can then consume
// this token to be given access to the extension resource.
class SEATBELT_EXPORT SeatbeltExtension {
 public:
  enum Type {
    // Requires (allow file-read* (extension "com.apple.app-sandbox.read")).
    FILE_READ,

    // Requires: (allow file-read* file-write*
    //               (extension "com.apple.app-sandbox.read-write"))
    FILE_READ_WRITE,

    // TODO(rsesek): Potentially support MACH and GENERIC extension types.
  };

  SeatbeltExtension(const SeatbeltExtension&) = delete;
  SeatbeltExtension& operator=(const SeatbeltExtension&) = delete;

  // Before an extension is destroyed, it must be consumed or explicitly
  // revoked.
  ~SeatbeltExtension();

  // Issues a sandbox extension of the specified |type|, to grant access to
  // the |resource| of that class. This returns the resulting token that can
  // be used to construct an extension object for consumption, or null if
  // issuing the token failed.
  static std::unique_ptr<SeatbeltExtensionToken> Issue(
      Type type,
      const std::string& resource);

  // Constructs a sandbox extension from a token object. The token can then
  // be consumed or revoked.
  static std::unique_ptr<SeatbeltExtension> FromToken(
      SeatbeltExtensionToken token);

  // Consumes the sandbox extension, giving the calling process access to the
  // resource for which the extension was issued. Returns true if the
  // extension was consumed and the resource access is now permitted, and
  // false on error with the resource still denied. The extension must be
  // revoked by the calling process before being destructed.
  bool Consume();

  // Like Consume(), but makes it so that the extension cannot be revoked.
  bool ConsumePermanently();

  // Revokes access to the extension and the resource for which it was issued.
  // Returns true if the extension was revoked and false if not.
  //
  // A consuming process can revoke an extension at any time. Once an
  // extension is revoked, it can be re-acquired by creating a new extension
  // object from the token object.
  bool Revoke();

 private:
  explicit SeatbeltExtension(const std::string& token);

  // Creates the token for the sandbox extension type and resource.
  static char* IssueToken(Type type, const std::string& resource);

  // The extension token, empty if the extension has been consumed permanetly
  // or revoked.
  std::string token_;

  // An opaque reference to a consumed extension, 0 if revoked or not consumed.
  int64_t handle_;
};

}  // namespace sandbox

#endif  // SANDBOX_MAC_SEATBELT_EXTENSION_H_
